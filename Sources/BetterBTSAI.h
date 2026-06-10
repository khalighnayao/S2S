#pragma once

#ifndef BETTERBTSAI_H
#define BETTERBTSAI_H

#include "Stopwatch.h"

// Log-verbosity globals, shared by the per-subsystem tagged logs
// (CvWorkerAI [WAI/*] -> BuildEvaluation.log, CvHunterAI [HAI/*] -> HunterAI.log,
// CvDecisionAI [DAI/*] -> DecisionAI.log, ContractBroker -> ContractBroker.log).
// Set from BUG options in CvGlobals; see logDecisionAI et al. below.
extern int gPlayerLogLevel;
extern int gTeamLogLevel;
extern int gCityLogLevel;
extern int gUnitLogLevel;

// Turn-timing verbosity, driven by its OWN BUG option (Autolog__LogLevelPerf) so wall-clock
// timing can run always-on in normal play without enabling the verbose AI logs. 0 = off.
// Feeds logPerf() -> Performance.log. See PERF_SCOPE below.
extern int gPerfLogLevel;

void logAIJson(CvWString type, CvWString identifier, CvWString squirrel, CvWString message);
void logCB(CvString message);
void logToFile(CvString message, const char* filename);
void logContractBroker(int level, const char* format, ...);
void logBuildEvaluation(int level, const char* format, ...);
void logHunterAI(int level, const char* format, ...);
void logDecisionAI(int level, const char* format, ...);
void logDiploAI(int level, const char* format, ...);
void logWarAI(int level, const char* format, ...);
void logUnitAI(int level, const char* format, ...);
void logCityAI(int level, const char* format, ...);
void logGroupAI(int level, const char* format, ...);
void logEspionageAI(int level, const char* format, ...);
void logFoundAI(int level, const char* format, ...);
void logGameInfo(const char* format, ...);
void logCombatAI(int level, const char* format, ...);

// Engine-integrity warnings -- [ENG/*] tags -> Engine.log, gated by gTeamLogLevel.
// Home for "should never happen" sanity checks demoted from FAssert/FErrorMsg so they
// still surface in FinalRelease (where FASSERT compiles out) without popping dialogs or
// flooding Asserts.log with stack traces. One key=value line per occurrence; the
// repeated-line count is itself the signal (don't dedup).
//   [ENG/viscap] (lvl 2, CvPlot::changeVisibilityCount) = a team's plot visibility
//   count went negative and was capped to zero (fires en masse during
//   recalculateModifiers' remove/re-add sight passes).
void logEngine(int level, const char* format, ...);

// Wall-clock turn-phase timing -> Performance.log, gated by gPerfLogLevel.
// Works with ANY DLL config (Assert/Release); no special Profile build needed.
// Emits one grep-friendly line per phase:
//   [PERF/phase] turn=312 owner=1 phase=CvPlayer::doTurn ms=84.213
// (owner=-1 for game/map-scope phases that have no single player).
void logPerf(int level, const char* format, ...);

// RAII scope timer: starts a high-res stopwatch on construction and logs the elapsed
// milliseconds via logPerf() when it goes out of scope. Cost when gPerfLogLevel==0 is a
// single integer compare, so it is safe to leave compiled into shipping DLLs.
// Use the PERF_SCOPE macro rather than constructing directly.
class ScopedPerfTimer
{
public:
	ScopedPerfTimer(const char* szPhase, int iOwner);
	~ScopedPerfTimer();

private:
	win32::Stopwatch m_stopwatch;
	const char* m_szPhase;
	int m_iOwner;
	bool m_bActive;

	ScopedPerfTimer(const ScopedPerfTimer&);
	ScopedPerfTimer& operator=(const ScopedPerfTimer&);
};

#define PERF_SCOPE_CONCAT_(a, b) a##b
#define PERF_SCOPE_CONCAT(a, b) PERF_SCOPE_CONCAT_(a, b)
// Time the enclosing scope. szPhase is a stable label; iOwner is a player id (or -1).
#define PERF_SCOPE(szPhase, iOwner) ScopedPerfTimer PERF_SCOPE_CONCAT(perfTimer_, __LINE__)((szPhase), (iOwner))

// Accumulating scope timer: instead of logging, ADDS its elapsed ms to a caller-owned
// accumulator on scope exit. Use to measure interleaved sub-sections inside a hot loop --
// accumulate across iterations, then logPerf() the totals ONCE (avoids per-iteration log spam).
// Active only when gPerfLogLevel >= 1, so it is free when perf logging is off.
class PerfAccumTimer
{
public:
	explicit PerfAccumTimer(double& dAccumMs)
		: m_dAccumMs(dAccumMs)
		, m_bActive(gPerfLogLevel >= 1)
	{
		if (m_bActive) m_stopwatch.Start();
	}
	~PerfAccumTimer()
	{
		if (m_bActive)
		{
			m_stopwatch.Stop();
			m_dAccumMs += m_stopwatch.ElapsedMilliseconds();
		}
	}

private:
	win32::Stopwatch m_stopwatch;
	double& m_dAccumMs;
	bool m_bActive;

	PerfAccumTimer(const PerfAccumTimer&);
	PerfAccumTimer& operator=(const PerfAccumTimer&);
};
// Accumulate the enclosing scope's ms into dAccumMs (a double the caller declares + logs).
#define PERF_ACCUM(dAccumMs) PerfAccumTimer PERF_SCOPE_CONCAT(perfAccum_, __LINE__)(dAccumMs)

#endif