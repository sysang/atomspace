/*
 * Commands.cc
 * Fast command interpreter for basic AtomSpace commands.
 *
 * Copyright (C) 2020 Linas Vepstas
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

#include <time.h>

#include <functional>
#include <iomanip>
#include <string>

#include <opencog/atoms/atom_types/NameServer.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/value/FloatValue.h>
#include <opencog/atoms/truthvalue/TruthValue.h>
#include <opencog/atomspace/AtomSpace.h>

#include "Commands.h"
#include "Sexpr.h"

using namespace opencog;

/// The cogserver provides a network API to send/receive Atoms over
/// the internet. The actual API is that of the StorageNode (see the
/// wiki page https://wiki.opencog.org/w/StorageNode for details.)
/// The cogserver supports the full `StorageNode` API, and it uses
/// the code in this directory in order to make it fast.
///
/// To aid in performance, a very special set of about 15 scheme
/// functions have been hard-coded in C++, in the static function
/// `Commands::interpret_command()` below.  The goal is to avoid the
/// overhead of entry/exit into guile. This works because the cogserver
/// is guaranteed to send only these commands, and no others.
//

Commands::Commands(void)
{
	_multi_space = false;
}

Commands::~Commands()
{
}

/// Search for optional AtomSpace argument in `cmd` at `pos`.
/// If none is found, then return `as`
AtomSpace*
Commands::get_opt_as(const std::string& cmd, size_t& pos, AtomSpace* as)
{
	if (not _multi_space) return as;

	pos = cmd.find_first_not_of(" \n\t", pos);
	if (0 == cmd.compare(pos, 10, "(AtomSpace"))
	{
		_multi_space = true;
		Handle hasp = Sexpr::decode_frame(
			HandleCast(top_space), cmd, pos, _space_map);
		return (AtomSpace*) hasp.get();
	}
	return as;
}

std::string Commands::interpret_command(AtomSpace* as,
                                        const std::string& cmd)
{
	// Fast dispatch. There should be zero hash collisions
	// here. If there are, we are in trouble. (Well, if there
	// are collisions, pre-pend the paren, post-pend the space.)
	static const size_t space = std::hash<std::string>{}("cog-atomspace)");
	static const size_t clear = std::hash<std::string>{}("cog-atomspace-clear)");
	static const size_t cache = std::hash<std::string>{}("cog-execute-cache!");
	static const size_t extra = std::hash<std::string>{}("cog-extract!");
	static const size_t recur = std::hash<std::string>{}("cog-extract-recursive!");
	static const size_t gtatm = std::hash<std::string>{}("cog-get-atoms");
	static const size_t incty = std::hash<std::string>{}("cog-incoming-by-type");
	static const size_t incom = std::hash<std::string>{}("cog-incoming-set");
	static const size_t keys = std::hash<std::string>{}("cog-keys->alist");
	static const size_t link = std::hash<std::string>{}("cog-link");
	static const size_t node = std::hash<std::string>{}("cog-node");
	static const size_t stval = std::hash<std::string>{}("cog-set-value!");
	static const size_t svals = std::hash<std::string>{}("cog-set-values!");
	static const size_t settv = std::hash<std::string>{}("cog-set-tv!");
	static const size_t value = std::hash<std::string>{}("cog-value");
	static const size_t dfine = std::hash<std::string>{}("define");

	// Find the command and dispatch
	size_t pos = cmd.find_first_not_of(" \n\t");
	if (std::string::npos == pos) return "";

	// Ignore comments
	if (';' == cmd[pos]) return "";

	if ('(' != cmd[pos])
		throw SyntaxException(TRACE_INFO, "Badly formed command: %s",
			cmd.c_str());

	pos ++; // Skip over the open-paren

	size_t epos = cmd.find_first_of(" \n\t", pos);
	if (std::string::npos == epos)
		throw SyntaxException(TRACE_INFO, "Not a command: %s",
			cmd.c_str());

	size_t act = std::hash<std::string>{}(cmd.substr(pos, epos-pos));

	// -----------------------------------------------
	// (cog-atomspace)
	if (space == act)
	{
		if (not top_space) return "()\n";
		return top_space->to_string("");
	}

	// -----------------------------------------------
	// (cog-atomspace-clear)
	if (clear == act)
	{
		as->clear();
		return "#t\n";
	}

	// -----------------------------------------------
	// (cog-execute-cache! (GetLink ...) (Predicate "key") ...)
	// This is complicated, and subject to change...
	if (cache == act)
	{
		pos = epos + 1;
		Handle query = Sexpr::decode_atom(cmd, pos, _space_map);
		query = as->add_atom(query);
		Handle key = Sexpr::decode_atom(cmd, ++pos, _space_map);
		key = as->add_atom(key);

		bool force = false;
		pos = cmd.find_first_of('(', pos);
		if (std::string::npos != pos)
		{
			Handle meta = Sexpr::decode_atom(cmd, pos, _space_map);
			meta = as->add_atom(meta);

			// XXX Hacky .. store time in float value...
			as->set_value(query, meta, createFloatValue((double)time(0)));
			if (std::string::npos != cmd.find("#t", pos))
				force = true;
		}
		ValuePtr rslt = query->getValue(key);
		if (nullptr != rslt and not force)
			return Sexpr::encode_value(rslt);

		// For now, prevent general execution.
		Type qt = query->get_type();
		if (not nameserver().isA(qt, PATTERN_LINK) and
		    not nameserver().isA(qt, JOIN_LINK))
			return "#f\n";

		rslt = query->execute();
		as->set_value(query, key, rslt);

		return Sexpr::encode_value(rslt);
	}

	// -----------------------------------------------
	// (cog-extract! (Concept "foo"))
	if (extra == act)
	{
		pos = epos + 1;
		Handle h = as->get_atom(Sexpr::decode_atom(cmd, pos, _space_map));
		if (nullptr == h) return "#t\n";
		if (as->extract_atom(h, false)) return "#t\n";
		return "#f\n";
	}

	// -----------------------------------------------
	// (cog-extract-recursive! (Concept "foo"))
	if (recur == act)
	{
		pos = epos + 1;
		Handle h = as->get_atom(Sexpr::decode_atom(cmd, pos, _space_map));
		if (nullptr == h) return "#t\n";
		if (as->extract_atom(h, true)) return "#t\n";
		return "#f\n";
	}

	// -----------------------------------------------
	// (cog-get-atoms 'Node #t)
	if (gtatm == act)
	{
		pos = epos + 1;
		Type t = Sexpr::decode_type(cmd, pos);

		pos = cmd.find_first_not_of(") \n\t", pos);
		bool get_subtypes = false;
		if (std::string::npos != pos and cmd.compare(pos, 2, "#f"))
			get_subtypes = true;

		// as = get_opt_as(cmd, pos, as);

		std::string rv = "(";
		HandleSeq hset;
		if (_multi_space and top_space)
			top_space->get_handles_by_type(hset, t, get_subtypes);
		else
			as->get_handles_by_type(hset, t, get_subtypes);
		for (const Handle& h: hset)
			rv += Sexpr::encode_atom(h, _multi_space);
		rv += ")";
		return rv;
	}

	// -----------------------------------------------
	// (cog-incoming-by-type (Concept "foo") 'ListLink)
	if (incty == act)
	{
		pos = epos + 1;
		Handle h = Sexpr::decode_atom(cmd, pos, _space_map);
		Type t = Sexpr::decode_type(cmd, pos);

		as = get_opt_as(cmd, pos, as);
		h = as->add_atom(h);

		std::string alist = "(";
		for (const Handle& hi : h->getIncomingSetByType(t))
			alist += Sexpr::encode_atom(hi);

		alist += ")\n";
		return alist;
	}

	// -----------------------------------------------
	// (cog-incoming-set (Concept "foo"))
	if (incom == act)
	{
		pos = epos + 1;
		Handle h = Sexpr::decode_atom(cmd, pos, _space_map);
		as = get_opt_as(cmd, pos, as);
		h = as->add_atom(h);
		std::string alist = "(";
		for (const Handle& hi : h->getIncomingSet())
			alist += Sexpr::encode_atom(hi);

		alist += ")\n";
		return alist;
	}

	// -----------------------------------------------
	// (cog-keys->alist (Concept "foo"))
	if (keys == act)
	{
		pos = epos + 1;
		Handle h = Sexpr::decode_atom(cmd, pos, _space_map);
		as = get_opt_as(cmd, pos, as);
		h = as->add_atom(h);

		std::string alist = "(";
		for (const Handle& key : h->getKeys())
		{
			alist += "(" + Sexpr::encode_atom(key) + " . ";
			alist += Sexpr::encode_value(h->getValue(key)) + ")";
		}
		alist += ")\n";
		return alist;
	}

	// -----------------------------------------------
	// (cog-node 'Concept "foobar")
	// (cog-link 'ListLink (Atom) (Atom) (Atom))
	if (node == act or link == act)
	{
		pos = epos + 1;
		Type t = Sexpr::decode_type(cmd, pos);

		Handle h;
		if (node == act)
		{
			size_t l = pos+1;
			size_t r = cmd.size();
			std::string name = Sexpr::get_node_name(cmd, l, r, t);
			as = get_opt_as(cmd, r, as);
			h = as->get_node(t, std::move(name));
		}
		else
		if (link == act)
		{
			HandleSeq outgoing;
			size_t l = pos+1;
			size_t r = cmd.size();
			while (l < r and ')' != cmd[l])
			{
				size_t l1 = l;
				size_t r1 = r;
				Sexpr::get_next_expr(cmd, l1, r1, 0);
				if (l1 == r1) break;
				outgoing.push_back(Sexpr::decode_atom(cmd, l1, r1, 0, _space_map));
				l = r1 + 1;
				pos = r1;
			}
			as = get_opt_as(cmd, pos, as);
			h = as->get_link(t, std::move(outgoing));
		}
		if (nullptr == h) return "()\n";
		return Sexpr::encode_atom(h, _multi_space);
	}

	// -----------------------------------------------
	// (cog-set-value! (Concept "foo") (Predicate "key") (FloatValue 1 2 3))
	if (stval == act)
	{
		pos = epos + 1;
		Handle atom = Sexpr::decode_atom(cmd, pos, _space_map);
		Handle key = Sexpr::decode_atom(cmd, ++pos, _space_map);
		ValuePtr vp = Sexpr::decode_value(cmd, ++pos);

		as = get_opt_as(cmd, pos, as);
		atom = as->add_atom(atom);
		key = as->add_atom(key);
		if (vp)
			vp = Sexpr::add_atoms(as, vp);
		as->set_value(atom, key, vp);
		return "()\n";
	}

	// -----------------------------------------------
	// (cog-set-values! (Concept "foo") (AtomSpace "foo")
	//     (alist (cons (Predicate "bar") (stv 0.9 0.8)) ...))
	if (svals == act)
	{
		pos = epos + 1;
		Handle h = Sexpr::decode_atom(cmd, pos, _space_map);
		pos++; // skip past close-paren

		if (not _multi_space)
		{
			// Search for optional AtomSpace argument
			as = get_opt_as(cmd, pos, as);
			h = as->add_atom(h);
		}
		Sexpr::decode_slist(h, cmd, pos);

		return "()\n";
	}

	// -----------------------------------------------
	// (cog-set-tv! (Concept "foo") (stv 1 0))
	// (cog-set-tv! (Concept "foo") (stv 1 0) (AtomSpace "foo"))
	if (settv == act)
	{
		pos = epos + 1;
		Handle h = Sexpr::decode_atom(cmd, pos, _space_map);
		ValuePtr tv = Sexpr::decode_value(cmd, ++pos);

		// Search for optional AtomSpace argument
		as = get_opt_as(cmd, pos, as);

		Handle ha = as->add_atom(h);
		if (nullptr == ha) return "()\n"; // read-only atomspace.
		as->set_truthvalue(ha, TruthValueCast(tv));
		return "()\n";
	}

	// -----------------------------------------------
	// (cog-value (Concept "foo") (Predicate "key"))
	if (value == act)
	{
		pos = epos + 1;
		Handle atom = Sexpr::decode_atom(cmd, pos, _space_map);
		atom = as->add_atom(atom);
		Handle key = Sexpr::decode_atom(cmd, ++pos, _space_map);
		key = as->add_atom(key);

		ValuePtr vp = atom->getValue(key);
		return Sexpr::encode_value(vp);
	}

	// -----------------------------------------------
	// (define sym (AtomSpace "foo" (AtomSpace "bar") (AtomSpace "baz")))
	// Place the current atomspace at the bottom of the hierarchy.
	if (dfine == act)
	{
		_multi_space = true;

		// Extract the symbolic name after the define
		pos = cmd.find_first_not_of(" \n\t", epos);
		epos = cmd.find_first_of(" \n\t", pos);
		// std::string sym = cmd.substr(pos, epos-pos);

		pos = epos+1;

		// Decode the AtomSpace frames
		AtomSpacePtr asp(AtomSpaceCast(as));
		Handle hasp = Sexpr::decode_frame(
			HandleCast(asp), cmd, pos, _space_map);
		top_space = AtomSpaceCast(hasp);

		// Hacky...
		// _space_map.insert({sym, top_space});

		return "()\n";
	}

	// -----------------------------------------------

	throw SyntaxException(TRACE_INFO, "Command not supported: >>%s<<",
		cmd.substr(pos, epos-pos).c_str());
}

// ===================================================================
