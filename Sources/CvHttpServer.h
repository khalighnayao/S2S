#pragma once

#ifndef CV_HTTP_SERVER_H
#define CV_HTTP_SERVER_H

// Minimal GET-only HTTP/1.0 dev server -- proof of concept for the live game-state
// endpoint (#387). Gated by the BUG option Autolog__HttpServer (Logging tab, off by
// default); binds 127.0.0.1:7227 only, so it is never reachable from off-machine.
// Every GET is answered with "hello world"; every other method gets 405 (Allow: GET).
//
// Runs on its own Win32 thread. HARD CONSTRAINT for future evolution: that thread
// must NEVER touch live game objects -- real data exposure must serve immutable
// snapshots published by the game thread (the derived-data repositories; see
// docs/plans/derived-data-repository.md section 8b).
namespace CvHttpServer
{
	// Start or stop the server thread to match the BUG option. Idempotent.
	// Call from the game thread only (wired into cvInternalGlobals::refreshOptionsBUG,
	// so toggling the option in the BUG screen takes effect on closing it).
	void setEnabled(bool bEnable);
}

#endif
