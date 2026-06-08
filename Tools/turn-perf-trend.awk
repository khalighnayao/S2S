# turn-perf-trend.awk -- flag per-turn time creep ("slower and slower") in Performance.log.
#
# Sums one [PERF/phase] label per game-turn, fits a least-squares line
# ms = a + b*turn over the session, and reports the slope b (ms added per
# turn), the per-turn % growth, R^2 (how linear/trustworthy the trend is),
# and a CREEP / weak / flat verdict. This is the automatic creep check that
# turns the "does it go slower and slower?" question into a number.
#
# Usage (Git Bash / any awk):
#   awk -f Tools/turn-perf-trend.awk "$LOGS/Performance.log"
#   awk -f Tools/turn-perf-trend.awk -v phase=total       "$LOGS/Performance.log"
#   awk -f Tools/turn-perf-trend.awk -v phase=doTurn.cities -v lo=1300 -v hi=1360 "$LOGS/Performance.log"
# where $LOGS = "$USERPROFILE/Documents/My Games/Beyond The Sword/Logs".
#
#   -v phase=NAME   which phase to trend. Default CvPlayer::doTurn (the AI
#                   player loop). Special value "total" = CvPlayer::doTurn +
#                   CvGame::doTurn per turn (the real per-turn wall clock).
#   -v lo=N -v hi=N restrict to game-turns [lo,hi] (optional).
#
# A phase can log many lines per turn (one per player/city); they are summed
# per turn first, then regressed. Field layout (default FS):
#   [timestamp] [PERF/phase] turn=N owner=M phase=X ms=Y   -> $3,$5,$6
BEGIN { if (phase == "") phase = "CvPlayer::doTurn" }

/PERF\/phase/ {
	t  = substr($3, 6) + 0          # turn=  -> drop "turn="
	ph = substr($5, 7)              # phase= -> drop "phase="
	ms = substr($6, 4) + 0          # ms=    -> drop "ms="
	if (lo != "" && t < lo) next
	if (hi != "" && t > hi) next
	if (phase == "total") {
		if (ph == "CvPlayer::doTurn" || ph == "CvGame::doTurn") { y[t] += ms; seen[t] = 1 }
	} else if (ph == phase) {
		y[t] += ms; seen[t] = 1
	}
}

END {
	# Gather and sort the turns we saw.
	n = 0
	for (t in seen) tt[++n] = t + 0
	for (i = 1; i <= n; i++)
		for (j = i + 1; j <= n; j++)
			if (tt[j] < tt[i]) { tmp = tt[i]; tt[i] = tt[j]; tt[j] = tmp }
	if (n < 3) { printf "need >=3 complete turns to trend; have %d\n", n; exit 1 }

	# Drop a trailing partial turn (game quit mid-turn): if the last turn's
	# total is < 40%% of the previous turn's, it is incomplete -- exclude it.
	last = tt[n]; prev = tt[n - 1]
	if (y[prev] > 0 && y[last] < 0.4 * y[prev]) {
		printf "# dropping partial trailing turn %d (ms=%.0f vs %.0f)\n", last, y[last], y[prev]
		n--
	}

	# Least-squares fit ms = a + b*turn.
	for (i = 1; i <= n; i++) {
		x = tt[i]; v = y[x]
		sx += x; sy += v; sxx += x * x; sxy += x * v; syy += v * v
	}
	denom = n * sxx - sx * sx
	if (denom == 0) { print "degenerate (all turns equal?)"; exit 1 }
	b    = (n * sxy - sx * sy) / denom     # slope, ms/turn
	mean = sy / n
	rden = denom * (n * syy - sy * sy)
	r2   = (rden > 0) ? ((n * sxy - sx * sy) ^ 2) / rden : 0
	pct  = (mean > 0) ? b / mean * 100 : 0

	# Verdict: CREEP needs a positive slope that is both materially large
	# (>=1%%/turn of the mean) and reasonably linear (R^2>=0.3); a near-zero
	# slope is flat; anything between is noise we shouldn't act on yet.
	if (b > 0 && pct >= 1.0 && r2 >= 0.3)      verdict = "CREEP"
	else if (pct < 0.3 && pct > -0.3)          verdict = "flat"
	else                                       verdict = "weak/noisy"

	printf "phase            : %s\n", phase
	printf "turns            : %d..%d (%d complete)\n", tt[1], tt[n], n
	printf "mean ms/turn     : %.0f\n", mean
	printf "slope            : %+.1f ms/turn  (%+.2f%%/turn of mean)\n", b, pct
	printf "projected +50t   : %+.0f ms  (%+.1fs)\n", b * 50, b * 50 / 1000
	printf "R^2 (linearity)  : %.2f\n", r2
	printf "verdict          : %s\n", verdict
}
