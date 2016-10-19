/*
 * opencog/atoms/base/Link.cc
 *
 * Copyright (C) 2008-2010 OpenCog Foundation
 * Copyright (C) 2002-2007 Novamente LLC
 * All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License v3 as
 * published by the Free Software Foundation and including the exceptions
 * at http://opencog.org/wiki/Licenses
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program; if not, write to:
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdio.h>

#include <opencog/util/exceptions.h>
#include <opencog/util/Logger.h>

#include <opencog/atoms/base/ClassServer.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atomspace/AtomTable.h>

#include <boost/range/algorithm.hpp>

#include "Link.h"

//#define DPRINTF printf
#define DPRINTF(...)

using namespace opencog;

void Link::resort(void)
{
    // Caution: this comparison function MUST BE EXACTLY THE SAME as
    // the one in AtomTable.cc, used for Unordered links. Changing
    // this without changing the other one will break things!
    std::sort(_outgoing.begin(), _outgoing.end(), handle_less());
}

void Link::init(const HandleSeq& outgoingVector)
{
    if (not classserver().isA(_type, LINK)) {
        throw InvalidParamException(TRACE_INFO,
            "Link ctor: Atom type is not a Link: '%d' %s.",
            _type, classserver().getTypeName(_type).c_str());
    }

    _outgoing = outgoingVector;
    // If the link is unordered, it will be normalized by sorting the
    // elements in the outgoing list.
    if (classserver().isA(_type, UNORDERED_LINK)) {
        resort();
    }
}

Link::~Link()
{
    DPRINTF("Deleting link:\n%s\n", this->toString().c_str());
}

std::string Link::toShortString(const std::string& indent) const
{
    std::stringstream answer;
    std::string more_indent = indent + "  ";

    answer << indent << "(" << classserver().getTypeName(_type);
    if (not getTruthValue()->isDefaultTV())
        answer << " " << getTruthValue()->toString();
    answer << "\n";

    // Here the target string is made. If a target is a node, its name is
    // concatenated. If it's a link, all its properties are concatenated.
    for (const Handle& h : _outgoing) {
        if (h.operator->() != NULL)
            answer << h->toShortString(more_indent);
        else
            answer << more_indent << "Undefined Atom!\n";
    }

    answer << indent << ") ; [" << _uuid << "]";

    if (_atomTable)
        answer << "[" << _atomTable->get_uuid() << "]\n";
    else
        answer << "[NULL]\n";

    return answer.str();
}

std::string Link::toString(const std::string& indent) const
{
    std::string answer = indent;
    std::string more_indent = indent + "  ";

    answer += "(" + classserver().getTypeName(_type);

    // Print the TV and AV only if its not the default.
    if (not getAttentionValue()->isDefaultAV())
        answer += " (av " +
             std::to_string(getAttentionValue()->getSTI()) + " " +
             std::to_string(getAttentionValue()->getLTI()) + " " +
             std::to_string(getAttentionValue()->getVLTI()) + ")";

    if (not getTruthValue()->isDefaultTV())
        answer += " " + getTruthValue()->toString();

    answer += "\n";
    // Here, the outset string is made. If a target is a node,
    // its name is concatenated. If it's a link, then recurse.
    for (const Handle& h : _outgoing) {
        if (h.operator->() != NULL)
            answer += h->toString(more_indent);
        else
            answer += more_indent + "Undefined Atom!\n";
    }

    answer += indent + ") ; [" +
            std::to_string(_uuid) + "][" +
            std::to_string(_atomTable? _atomTable->get_uuid() : -1) +
            "]\n";

    return answer;
}

bool Link::operator==(const Atom& other) const
{
    // Rule out obvious mis-matches, based on the hash.
    if (get_hash() != other.get_hash()) return false;

    if (getType() != other.getType()) return false;
    const Link& olink = dynamic_cast<const Link&>(other);

    Arity arity = getArity();
    if (arity != olink.getArity()) return false;

    // If the type is unordered and one of the uuids are invalid we
    // need to reorder by content to be sure that the children are
    // aligned.
// XXX this is just plain wrong .. its the wrong place to do this fix.
    if (classserver().isA(getType(), UNORDERED_LINK) and
        (_uuid != Handle::INVALID_UUID
         or other.getUUID() != Handle::INVALID_UUID))
    {
        HandleSeq sorted_outgoing(_outgoing),
            other_sorted_outgoing(olink._outgoing);
        boost::sort(sorted_outgoing, content_based_handle_less());
        boost::sort(other_sorted_outgoing, content_based_handle_less());
        return outgoings_equal(sorted_outgoing, other_sorted_outgoing);
    }

    // No need to reorder, compare the children directly
    return outgoings_equal(_outgoing, olink._outgoing);
}

bool Link::outgoings_equal(const HandleSeq& lhs, const HandleSeq& rhs)
{
    if (lhs.size() != rhs.size()) return false;

    for (Arity i = 0; i < lhs.size(); i++)
    {
        // TODO: may be replace this by
        //
        //     if (lhs[i] != rhs[i])
        //         return false;
        //
        // once Handle::operator== is fixed when comparing defined and
        // undefined handles.

        if (lhs[i]->operator != (*(rhs[i].const_atom_ptr())))
            return false;
    }
    return true;
}

bool Link::operator<(const Atom& other) const
{
    if (getType() == other.getType()) {
        const HandleSeq& outgoing = getOutgoingSet();
        const HandleSeq& other_outgoing = other.getOutgoingSet();
        Arity arity = outgoing.size();
        Arity other_arity = other_outgoing.size();
        if (arity == other_arity) {
            Arity i = 0;
            while (i < arity) {
                Handle ll = outgoing[i];
                Handle rl = other_outgoing[i];
                if (ll == rl)
                    i++;
                else
                    return ll->operator<(*rl.atom_ptr());
            }
            return false;
        } else
            return arity < other_arity;
    } else
        return getType() < other.getType();
}

/// Returns a Merkle tree hash -- that is, the hash of this link
/// chains the hash values of the child atoms, as well.
ContentHash Link::compute_hash() const
{
	// djb hash
	ContentHash hsh = 5381;
	hsh += (hsh <<5) + getType();

	for (const Handle& h: _outgoing)
	{
		hsh += (hsh <<5) + h->get_hash(); // recursive!
	}

	// Links will always have the MSB set.
	ContentHash mask = ((ContentHash) 1UL) << (8*sizeof(ContentHash) - 1);
	hsh |= mask;

	if (Handle::INVALID_HASH == hsh) hsh -= 1;
	_content_hash = hsh;
	return _content_hash;
}
