#pragma once
#ifndef CV_DERIVED_DATA_H
#define CV_DERIVED_DATA_H

//
//	Derived-data repositories -- four levels: Game > Team > Player > City.
//	Architecture: Sources/docs/plans/derived-data-repository.md.
//
//	Each repository holds memoized, CHANGE-DRIVEN derived AI data for its scope: computed lazily and
//	refreshed only when an event that affects it occurs, instead of the per-turn "wipe everything and
//	rebuild from scratch" the AI does today. Goals: (1) only recompute what changed, (2) one API
//	surface for reuse, (3) de-duplicate the hand-rolled re-derivations scattered across the AI files,
//	(4) shrink those files by moving the logic here.
//
//	THIS FILE IS THE STRUCTURE ONLY -- no data members yet. Data is added case by case, each datum
//	placed at the HIGHEST level where its inputs are invariant (a tech fact at Team, a civic value at
//	Player, the constructible set at City), so one recompute serves everything below it.
//
//	To add a datum to a level repository:
//	  1. add a `TLazy<T> m_xxx;` member;
//	  2. add a getter `const T& getXxx()` that lazily recomputes when dirty (pattern below);
//	  3. invalidate it from the relevant event endpoint(s) wired to the mutation sites.
//	Rules (see the plan): repository data is ADVISORY only -- never synced game state, never an input
//	to control flow that could spin (that is why the building-VALUE cache could hang but an input set
//	cannot); rebuild on load; invalidateAll() is the coarse bounded-staleness backstop.
//

class CvGameAI;
class CvTeamAI;
class CvPlayerAI;
class CvCityAI;

//	Lazy/dirty holder for one derived value. Recompute pattern at the getter:
//	    if (m_foo.dirty()) m_foo.set(computeFoo());
//	    return m_foo.value();
template <typename T>
class TLazy
{
public:
	TLazy() : m_bDirty(true) {}

	bool     dirty() const   { return m_bDirty; }
	void     invalidate()    { m_bDirty = true; }
	const T& value() const   { return m_value; }      // only meaningful when !dirty()
	void     set(const T& v) { m_value = v; m_bDirty = false; }

private:
	T    m_value;
	bool m_bDirty;
};

//	Common base for the four scope repositories. Parameterised by the owning AI object so each level
//	keeps a typed back-pointer (used later to recompute from owner state and to reach the level
//	above: City -> Player -> Team -> Game). Empty for now apart from the lifecycle hooks.
template <typename TOwner>
class CvDataRepository
{
public:
	CvDataRepository() : m_pOwner(NULL) {}

	void    init(TOwner* pOwner) { m_pOwner = pOwner; }
	TOwner* owner() const        { return m_pOwner; }

	//	Drop all derived data (game-init / load): mark every datum dirty. No-op until data exists.
	void reset() {}
	//	Coarse backstop -- invalidate everything. No-op until data exists.
	void invalidateAll() {}

protected:
	TOwner* m_pOwner;
};

//	-- Game level: static XML-derived data (prereq / enabler reverse-indices), built once at load --
class CvGameDataRepository   : public CvDataRepository<CvGameAI>   {};

//	-- Team level: tech / obsolete-building / war-shared facts --
class CvTeamDataRepository   : public CvDataRepository<CvTeamAI>   {};

//	-- Player level: tech / civic / promotion / bonus values, counts, area unit-AI counts --
class CvPlayerDataRepository : public CvDataRepository<CvPlayerAI> {};

//	-- City level: constructible set, building values, the city's declared needs --
class CvCityDataRepository   : public CvDataRepository<CvCityAI>   {};

#endif // CV_DERIVED_DATA_H
