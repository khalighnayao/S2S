#include "CvGameCoreDLL.h"
#include "CvHttpServer.h"
#include "CvGameAI.h"
#include "CvGlobals.h"
#include "CvInfos.h"
#include "CvPlayerAI.h"
#include "CvSelectionGroup.h"
#include "CvUnit.h"

// Deliberately the winsock 1.1 header, NOT winsock2.h: some unity batches pull a
// full-fat windows.h (no WIN32_LEAN_AND_MEAN) which includes winsock.h, and
// winsock2.h cannot be included after it (sockaddr redefinition in ws2def.h).
// winsock.h is immune to that under any unity reshuffle -- it is a guarded no-op
// when already present and self-sufficient when not. Everything used here is 1.1
// API, and ws2_32.dll exports all of it.
#include <winsock.h>

#ifndef SD_SEND
#define SD_SEND 1 // winsock.h omits the SD_* shutdown() constants (winsock2-only)
#endif

// Already on the linker LIBPATH (vendored Windows SDK v6.0); no fbuild.bff change needed.
#pragma comment(lib, "ws2_32.lib")

namespace
{
	const unsigned short HTTP_PORT = 7227;
	const int REQUEST_CAP = 4096;
	const int TARGET_CAP = 512;          // path + query taken from the request line
	const DWORD PUBLISH_INTERVAL_MS = 5000;

	HANDLE g_hThread = NULL;
	volatile LONG g_iStopRequested = 0;

	// --- Published snapshot -------------------------------------------------------
	// Written by the game thread (publishIfDue), read by the server thread. The lock
	// is initialized before the server thread can exist (setEnabled) and publish
	// early-outs while the server is off, so neither side touches it uninitialized.
	struct UnitSnap
	{
		int iID;
		int iOwner;
		int iX;
		int iY;
		int iGroup;
		int iMissionAI;
		int iActivity;
		int iDamage;
		int iLevel;
		CvString szType; // XML key, e.g. UNIT_WARDOG
		CvString szAI;   // XML key, e.g. UNITAI_HUNTER
	};

	struct GameSnapshot
	{
		GameSnapshot() : iTurn(-1) {}
		int iTurn;
		std::vector<UnitSnap> units;
	};

	CRITICAL_SECTION g_snapshotLock;
	bool g_bLockInitialized = false;
	// The lock only ever guards the pointer copy/swap (nanoseconds on both sides);
	// readers render from their own refcounted reference to the immutable snapshot,
	// so a slow full-dump render can never stall the game thread's publish.
	bst::shared_ptr<const GameSnapshot> g_pSnapshot; // guarded by g_snapshotLock
	DWORD g_iLastPublishTick = 0;                    // game thread only

	bst::shared_ptr<const GameSnapshot> grabSnapshot()
	{
		EnterCriticalSection(&g_snapshotLock);
		const bst::shared_ptr<const GameSnapshot> pSnap = g_pSnapshot;
		LeaveCriticalSection(&g_snapshotLock);
		return pSnap;
	}

	// --- Socket plumbing ----------------------------------------------------------

	const char RESPONSE_405[] =
		"HTTP/1.0 405 Method Not Allowed\r\n"
		"Allow: GET\r\n"
		"Content-Type: text/plain\r\n"
		"Content-Length: 9\r\n"
		"Connection: close\r\n"
		"\r\n"
		"GET only\n";

	void sendAll(SOCKET sock, const char* szData, int iLen)
	{
		int iSent = 0;
		while (iSent < iLen)
		{
			const int iRet = send(sock, szData + iSent, iLen - iSent, 0);
			if (iRet == SOCKET_ERROR || iRet == 0)
			{
				return;
			}
			iSent += iRet;
		}
	}

	void sendResponse(SOCKET sock, const char* szStatus, const char* szContentType, const CvString& szBody, int iTurn)
	{
		const CvString szHead = CvString::format(
			"HTTP/1.0 %s\r\n"
			"Content-Type: %s\r\n"
			"Content-Length: %u\r\n"
			"X-S2S-Turn: %d\r\n"
			"Connection: close\r\n"
			"\r\n",
			szStatus, szContentType, (unsigned int)szBody.size(), iTurn);
		sendAll(sock, szHead.c_str(), (int)szHead.size());
		sendAll(sock, szBody.c_str(), (int)szBody.size());
	}

	// --- Request handling (server thread; snapshot reads only) ---------------------

	// Renders the /units document from this thread's own snapshot reference.
	// IMPORTANT: the document is assembled by CONCATENATION -- CvString::format's
	// growth loop caps out around 82KB (40 x 2KB attempts in formatv) and silently
	// returns an EMPTY string beyond that, and a full dump of a mature game is
	// megabytes. format() is only safe here for the small per-unit pieces.
	CvString renderUnits(bool bFilterId, int iId, bool bFilterOwner, int iOwner)
	{
		const bst::shared_ptr<const GameSnapshot> pSnap = grabSnapshot();
		const int iTurn = pSnap ? pSnap->iTurn : -1;

		CvString szItems;
		int iCount = 0;
		if (pSnap)
		{
			szItems.reserve(pSnap->units.size() * 160);
			for (size_t i = 0; i < pSnap->units.size(); ++i)
			{
				const UnitSnap& u = pSnap->units[i];
				if (bFilterId && u.iID != iId)
				{
					continue;
				}
				if (bFilterOwner && u.iOwner != iOwner)
				{
					continue;
				}
				if (iCount > 0)
				{
					szItems += ",";
				}
				szItems += CvString::format(
					"\n{\"id\":%d,\"owner\":%d,\"x\":%d,\"y\":%d,\"type\":\"%s\",\"ai\":\"%s\","
					"\"group\":%d,\"missionAI\":%d,\"activity\":%d,\"damage\":%d,\"level\":%d}",
					u.iID, u.iOwner, u.iX, u.iY, u.szType.c_str(), u.szAI.c_str(),
					u.iGroup, u.iMissionAI, u.iActivity, u.iDamage, u.iLevel);
				iCount++;
			}
		}

		CvString szBody = CvString::format("{\"turn\":%d,\"count\":%d,\"units\":[", iTurn, iCount);
		szBody += szItems;
		szBody += "\n]}\n";
		return szBody;
	}

	int snapshotTurn()
	{
		const bst::shared_ptr<const GameSnapshot> pSnap = grabSnapshot();
		return pSnap ? pSnap->iTurn : -1;
	}

	void handleRequest(SOCKET sock, const char* szRequest)
	{
		if (strncmp(szRequest, "GET ", 4) != 0)
		{
			sendAll(sock, RESPONSE_405, (int)strlen(RESPONSE_405));
			return;
		}

		// Extract the request target ("/units?id=123") -- everything between the
		// method and the next space/CR, length-capped.
		char szTarget[TARGET_CAP + 1];
		int iLen = 0;
		for (const char* p = szRequest + 4; *p != '\0' && *p != ' ' && *p != '\r' && *p != '\n' && iLen < TARGET_CAP; ++p)
		{
			szTarget[iLen++] = *p;
		}
		szTarget[iLen] = '\0';

		// Split path from query string.
		char* szQuery = strchr(szTarget, '?');
		if (szQuery != NULL)
		{
			*szQuery = '\0';
			++szQuery;
		}

		if (strcmp(szTarget, "/") == 0)
		{
			sendResponse(sock, "200 OK", "text/plain", CvString("hello world\n"), snapshotTurn());
		}
		else if (strcmp(szTarget, "/units") == 0)
		{
			// Parse the supported filters; unknown parameters are ignored.
			bool bFilterId = false, bFilterOwner = false;
			int iId = -1, iOwner = -1;
			char* szTok = szQuery;
			while (szTok != NULL && *szTok != '\0')
			{
				char* szNext = strchr(szTok, '&');
				if (szNext != NULL)
				{
					*szNext = '\0';
					++szNext;
				}
				if (strncmp(szTok, "id=", 3) == 0)
				{
					bFilterId = true;
					iId = atoi(szTok + 3);
				}
				else if (strncmp(szTok, "playerNumber=", 13) == 0)
				{
					bFilterOwner = true;
					iOwner = atoi(szTok + 13);
				}
				szTok = szNext;
			}
			sendResponse(sock, "200 OK", "application/json", renderUnits(bFilterId, iId, bFilterOwner, iOwner), snapshotTurn());
		}
		else
		{
			sendResponse(sock, "404 Not Found", "application/json", CvString("{\"error\":\"not found\"}\n"), snapshotTurn());
		}
	}

	void handleClient(SOCKET sock)
	{
		// Bound every socket op so a stalled client cannot wedge the server thread
		// (and with it the option-off shutdown wait) for more than ~2s.
		const int iTimeoutMs = 2000;
		setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&iTimeoutMs, sizeof(iTimeoutMs));
		setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&iTimeoutMs, sizeof(iTimeoutMs));

		char szRequest[REQUEST_CAP + 1];
		int iReceived = 0;
		while (iReceived < REQUEST_CAP)
		{
			const int iRet = recv(sock, szRequest + iReceived, REQUEST_CAP - iReceived, 0);
			if (iRet == SOCKET_ERROR || iRet == 0)
			{
				break;
			}
			iReceived += iRet;
			szRequest[iReceived] = '\0';
			if (strstr(szRequest, "\r\n\r\n") != NULL)
			{
				break; // headers complete; the request line is all we need
			}
		}

		if (iReceived > 0)
		{
			szRequest[iReceived] = '\0';
			handleRequest(sock, szRequest);
			shutdown(sock, SD_SEND);
		}
		closesocket(sock);
	}

	void runServerLoop()
	{
		WSADATA wsaData;
		if (WSAStartup(MAKEWORD(1, 1), &wsaData) != 0)
		{
			return;
		}

		const SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (listenSock == INVALID_SOCKET)
		{
			WSACleanup();
			return;
		}

		sockaddr_in addr;
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // 127.0.0.1 only -- never exposed off-machine
		addr.sin_port = htons(HTTP_PORT);

		// boost::bind is visible at global scope via the PCH's using-directives and VC7.1
		// resolves even a qualified ::bind call to it. Select winsock's bind through a
		// typed function pointer instead -- the boost template cannot match this signature,
		// so the address-of uniquely picks the winsock function.
		int (PASCAL *pfnWinsockBind)(SOCKET, const struct sockaddr*, int) = &::bind;
		if (pfnWinsockBind(listenSock, (sockaddr*)&addr, (int)sizeof(addr)) == SOCKET_ERROR
			|| listen(listenSock, SOMAXCONN) == SOCKET_ERROR)
		{
			// Most likely the port is in use (e.g. a second game instance); nothing to serve.
			closesocket(listenSock);
			WSACleanup();
			return;
		}

		while (g_iStopRequested == 0)
		{
			// select() with a timeout so the stop flag is honoured within 250ms
			// without the game thread having to close the socket out from under us.
			fd_set readSet;
			FD_ZERO(&readSet);
			FD_SET(listenSock, &readSet);
			timeval timeout;
			timeout.tv_sec = 0;
			timeout.tv_usec = 250 * 1000;

			const int iReady = select(0, &readSet, NULL, NULL, &timeout);
			if (iReady == SOCKET_ERROR)
			{
				break;
			}
			if (iReady == 0)
			{
				continue;
			}
			const SOCKET clientSock = accept(listenSock, NULL, NULL);
			if (clientSock != INVALID_SOCKET)
			{
				handleClient(clientSock);
			}
		}

		closesocket(listenSock);
		WSACleanup();
	}

	DWORD WINAPI serverThreadEntry(LPVOID lpModule)
	{
		// SEH guard: a fault on this thread must never take the game down.
		__try
		{
			runServerLoop();
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
		}
		// Releases the DLL pin taken in setEnabled and exits in one step, so the EXE
		// can never unmap the DLL while this thread is still executing inside it.
		FreeLibraryAndExitThread((HMODULE)lpModule, 0);
	}
}

void CvHttpServer::setEnabled(bool bEnable)
{
	if (bEnable == (g_hThread != NULL))
	{
		return;
	}

	if (bEnable)
	{
		if (!g_bLockInitialized)
		{
			InitializeCriticalSection(&g_snapshotLock);
			g_bLockInitialized = true;
		}
		g_iLastPublishTick = 0; // force a fresh snapshot on the next frame

		// Pin the DLL for the thread's lifetime BEFORE starting it (the thread
		// releases the pin as it exits, via FreeLibraryAndExitThread).
		HMODULE hModule = NULL;
		if (!GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
			(LPCSTR)&serverThreadEntry, &hModule))
		{
			return;
		}
		InterlockedExchange(&g_iStopRequested, 0);
		g_hThread = CreateThread(NULL, 0, serverThreadEntry, (LPVOID)hModule, 0, NULL);
		if (g_hThread == NULL)
		{
			FreeLibrary(hModule);
		}
	}
	else
	{
		InterlockedExchange(&g_iStopRequested, 1);
		// The loop polls the flag every 250ms and client socket ops are capped at 2s,
		// so 5s only times out if something is genuinely wedged -- in which case we
		// leak the handle rather than TerminateThread a thread that owns a socket.
		if (WaitForSingleObject(g_hThread, 5000) == WAIT_OBJECT_0)
		{
			CloseHandle(g_hThread);
		}
		g_hThread = NULL;
	}
}

// Game thread, once per frame from CvGame::update. Walks live game objects HERE
// (the only thread allowed to) and swaps the result into the served snapshot.
void CvHttpServer::publishIfDue()
{
	if (g_hThread == NULL)
	{
		return; // server off -- this bool check is the entire cost
	}
	const DWORD iNow = GetTickCount();
	if (g_iLastPublishTick != 0 && iNow - g_iLastPublishTick < PUBLISH_INTERVAL_MS)
	{
		return;
	}
	g_iLastPublishTick = iNow;

	bst::shared_ptr<GameSnapshot> pNew(new GameSnapshot());
	pNew->iTurn = GC.getGame().getGameTurn();
	pNew->units.reserve(4096);

	for (int iI = 0; iI < MAX_PLAYERS; iI++)
	{
		const CvPlayerAI& kPlayer = GET_PLAYER((PlayerTypes)iI);
		if (!kPlayer.isAlive())
		{
			continue;
		}
		foreach_(const CvUnit* pLoopUnit, kPlayer.units())
		{
			UnitSnap snap;
			snap.iID = pLoopUnit->getID();
			snap.iOwner = pLoopUnit->getOwner();
			snap.iX = pLoopUnit->getX();
			snap.iY = pLoopUnit->getY();
			snap.iDamage = pLoopUnit->getDamage();
			snap.iLevel = pLoopUnit->getLevel();

			const UnitTypes eType = pLoopUnit->getUnitType();
			snap.szType = eType != NO_UNIT ? GC.getUnitInfo(eType).getType() : "NO_UNIT";

			const UnitAITypes eAI = pLoopUnit->AI_getUnitAIType();
			snap.szAI = eAI != NO_UNITAI ? GC.getUnitAIInfo(eAI).getType() : "NO_UNITAI";

			const CvSelectionGroup* pGroup = pLoopUnit->getGroup();
			snap.iGroup = pGroup != NULL ? pGroup->getID() : -1;
			snap.iMissionAI = pGroup != NULL ? pGroup->AI_getMissionAIType() : -1;
			snap.iActivity = pGroup != NULL ? pGroup->getActivityType() : -1;

			pNew->units.push_back(snap);
		}
	}

	EnterCriticalSection(&g_snapshotLock);
	g_pSnapshot = pNew;
	LeaveCriticalSection(&g_snapshotLock);
}
