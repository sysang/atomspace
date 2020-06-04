/*
 * PatternTerm.cc
 *
 * Copyright (C) 2016 OpenCog Foundation
 * All Rights Reserved
 *
 * Created by Nil Geisweiller Oct 2016
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include <opencog/util/oc_assert.h>
#include "PatternTerm.h"

namespace opencog {

const PatternTermPtr PatternTerm::UNDEFINED(std::make_shared<PatternTerm>());

PatternTerm::PatternTerm(void)
	: _handle(Handle::UNDEFINED),
	  _quote(Handle::UNDEFINED),
	  _parent(PatternTerm::UNDEFINED),
	  _has_any_bound_var(false),
	  _has_bound_var(false),
	  _has_any_globby_var(false),
	  _has_globby_var(false),
	  _has_any_evaluatable(false),
	  _has_evaluatable(false),
	  _has_any_unordered_link(false),
	  _is_literal(false)
{}

PatternTerm::PatternTerm(const PatternTermPtr& parent, const Handle& h)
	: _handle(h), _quote(Handle::UNDEFINED), _parent(parent),
	  _quotation(parent->_quotation.level(),
	             false /* necessarily false since it is local */),
	  _has_any_bound_var(false),
	  _has_bound_var(false),
	  _has_any_globby_var(false),
	  _has_globby_var(false),
	  _has_any_evaluatable(false),
	  _has_evaluatable(false),
	  _has_any_unordered_link(false),
	  _is_literal(false)
{
	Type t = h->get_type();

	// Discard the following QuoteLink, UnquoteLink or LocalQuoteLink
	// as it is serving its quoting or unquoting function.
	if (_quotation.consumable(t)) {
		if (1 != h->get_arity())
			throw InvalidParamException(TRACE_INFO,
			                            "QuoteLink/UnquoteLink/LocalQuoteLink has "
			                            "unexpected arity!");
		// Save the quotes, useful for mapping patterns to grounds
		_quote = _handle;
		_handle = h->getOutgoingAtom(0);
	}

	// Update the quotation state
	_quotation.update(t);
}

PatternTermPtr PatternTerm::addOutgoingTerm(const Handle& h)
{
	PatternTermPtr ptm(createPatternTerm(shared_from_this(), h));
	_outgoing.push_back(ptm);
	return ptm;
}

PatternTermSeq PatternTerm::getOutgoingSet() const
{
	PatternTermSeq oset;
	for (const PatternTermWPtr& w : _outgoing)
	{
		PatternTermPtr s(w.lock());
		OC_ASSERT(nullptr != s, "Unexpected corruption of PatternTerm oset!");
		oset.push_back(s);
	}

	return oset;
}

PatternTermPtr PatternTerm::getOutgoingTerm(Arity pos) const
{
	// Checks for a valid position
	if (pos < getArity()) {
		PatternTermPtr s(_outgoing[pos].lock());
		OC_ASSERT(nullptr != s, "Unexpected missing PatternTerm oset entry!");
		return s;
	} else {
		throw RuntimeException(TRACE_INFO,
		                       "invalid outgoing set index %d", pos);
	}
}

/**
 * isDescendant - return true if `this` is a lineal descendant of `ptm`.
 * That is, return true if `ptm` appears as a parent somewhere in the
 * chain of parents of `this`.
 *
 * Mnemonic device: child->isDescendant(parent) == true
 */
bool PatternTerm::isDescendant(const PatternTermPtr& ptm) const
{
	if (PatternTerm::UNDEFINED == _parent) return false;
	if (*_parent == *ptm) return true;
	return _parent->isDescendant(ptm);
}

// ==============================================================
/**
 * Equality operator.  Both the content must match, and the path
 * taken to get to the content must match.
 */
bool PatternTerm::operator==(const PatternTerm& other)
{
	if (_handle != other._handle) return false;
	if (_parent != other._parent) return false;
	if (PatternTerm::UNDEFINED == _parent) return true;

	return _parent->operator==(*other._parent);
}

// ==============================================================

// Mark recursively, all the way to the root.
void PatternTerm::addAnyBoundVar()
{
	if (not _has_any_bound_var)
	{
		_has_any_bound_var = true;
		if (_parent != PatternTerm::UNDEFINED)
			_parent->addAnyBoundVar();
	}
}

/// Set two flags: one flag (the "any" flag) is set recursively from
/// a variable, all the way up to the root, indicating that there's
/// a variable on this path.  The other flag gets set only on the
/// variable, and it's immediate parent (i.e. the holder of the
/// variable).
void PatternTerm::addBoundVariable()
{
	// Mark just this term (the variable itself)
	// and mark the term that holds us.
	_has_bound_var = true;
	if (_parent != PatternTerm::UNDEFINED)
			_parent->_has_bound_var = true;

	// Mark recursively, all the way to the root.
	addAnyBoundVar();
}

// ==============================================================
// Just like above, but for globs.

void PatternTerm::addAnyGlobbyVar()
{
	if (not _has_any_globby_var)
	{
		_has_any_globby_var = true;
		if (_parent != PatternTerm::UNDEFINED)
			_parent->addAnyGlobbyVar();
	}
}

void PatternTerm::addGlobbyVar()
{
	_has_globby_var = true;

	if (_parent != PatternTerm::UNDEFINED)
		_parent->_has_globby_var = true;

	addAnyGlobbyVar();
}

// ==============================================================
// Just like above, but for evaluatables.

void PatternTerm::addAnyEvaluatable()
{
	if (not _has_any_evaluatable)
	{
		_has_any_evaluatable = true;
		if (_parent != PatternTerm::UNDEFINED)
			_parent->addAnyEvaluatable();
	}
}

void PatternTerm::addEvaluatable()
{
	_has_evaluatable = true;

	if (_parent != PatternTerm::UNDEFINED)
		_parent->_has_evaluatable = true;

	addAnyEvaluatable();
}

// ==============================================================

void PatternTerm::addUnorderedLink()
{
	if (_has_any_unordered_link) return;

	_has_any_unordered_link = true;
	if (_parent != PatternTerm::UNDEFINED)
		_parent->addUnorderedLink();
}

// ==============================================================

void PatternTerm::markLiteral()
{
	if (_is_literal) return;

	_is_literal = true;
	for (PatternTermPtr& ptm : getOutgoingSet())
		ptm->markLiteral();
}

// ==============================================================

std::string PatternTerm::to_string() const { return to_string(": "); }

std::string PatternTerm::to_string(const std::string& indent) const
{
	// Term is null-terminated at thye top.
	// Top term never has a handle in it.
	if (not _handle) return "-";
	std::string str = _parent->to_string();
	str += indent + _handle->id_to_string();
	return str;
}

std::string oc_to_string(const PatternTerm& pt, const std::string& indent)
{
	return pt.to_string(indent);
}

std::string oc_to_string(const PatternTermPtr& pt_ptr, const std::string& indent)
{
	return pt_ptr->to_string();
}

} // ~namespace opencog
