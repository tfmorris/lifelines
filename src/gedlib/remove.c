/* 
   Copyright (c) 1991-1999 Thomas T. Wetmore IV

   Permission is hereby granted, free of charge, to any person
   obtaining a copy of this software and associated documentation
   files (the "Software"), to deal in the Software without
   restriction, including without limitation the rights to use, copy,
   modify, merge, publish, distribute, sublicense, and/or sell copies
   of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be
   included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
   NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
   BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
   ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.
*/
/*=============================================================
 * remove.c -- Remove child or spouse from family
 * Copyright(c) 1992-94 by T.T. Wetmore IV; all rights reserved
 * pre-SourceForge version information:
 *   2.3.4 - 24 Jun 93    2.3.5 - 01 Sep 93
 *   3.0.0 - 23 Sep 94    3.0.2 - 05 Dec 94
 *   3.0.3 - 15 Aug 95
 *===========================================================*/

#include "llstdlib.h"
#include "table.h"
#include "translat.h"
#include "gedcom.h"
#include "indiseq.h"
#include "liflines.h"
#include "feedback.h"

/*********************************************
 * external/imported variables
 *********************************************/

extern STRING qShaslnk;

/*********************************************
 * local function prototypes
 *********************************************/

static INT count_xrefs_in_list(STRING xref, NODE list);
static NODE remove_any_xrefs_node_list(STRING xref, NODE list);

/*================================================================
 * remove_indi -- Delete person and links; if this leaves families
 *   with no links, remove them.
 *   This should not fail.
 *  indi:  [in]  person to remove - (must be valid)
 * Created: 2001/11/08, Perry Rapp
 *==============================================================*/
void
remove_indi (NODE indi)
{
	STRING key;
	NODE name, refn, sex, body, famc, fams;
	NODE node, husb, wife, chil, rest, fam, fref;
	INT keyint;

	split_indi_old(indi, &name, &refn, &sex, &body, &famc, &fams);

/* Remove person from families he/she is in as a parent */

	for (node = fams; node; node = nsibling(node)) {
		fam = key_to_fam(rmvat(nval(node)));
		split_fam(fam, &fref, &husb, &wife, &chil, &rest);
		husb = remove_any_xrefs_node_list(nxref(indi), husb);
		wife = remove_any_xrefs_node_list(nxref(indi), wife);
		join_fam(fam, fref, husb, wife, chil, rest);
		if (husb || wife || chil)
			fam_to_dbase(fam);
		else
			remove_empty_fam(fam);
	}

/* Remove person from families he/she is in as a child */

	for (node = famc; node; node = nsibling(node)) { 
		fam = key_to_fam(rmvat(nval(node)));
		split_fam(fam, &fref, &husb, &wife, &chil, &rest);
		chil = remove_any_xrefs_node_list(nxref(indi), chil);
		join_fam(fam, fref, husb, wife, chil, rest);
		if (husb || wife || chil)
			fam_to_dbase(fam);
		else
			remove_empty_fam(fam);
	}

/* Add key to list of unused keys */
	key = rmvat(nxref(indi));
	keyint = atoi(key + 1);
	addixref(keyint);

/* Remove indi from cache */
	remove_indi_cache(key);

/* Remove any name and refn entries */
	for (node = name; node; node = nsibling(node))
		{
		remove_name(nval(node), key);
		}
	for (node = refn; node; node = nsibling(node))
		{
		if (nval(node))
			{
			remove_refn(nval(node), key);
			}
		}
/* Remove any entries in existing browse lists */
	remove_from_browse_lists(key);

/* Remove from on-disk database */
	del_in_dbase(key);

/* Delete the in-memory record we're holding (indi) */
	join_indi(indi, name, refn, sex, body, famc, fams);
	free_nodes(indi);
}
/*==========================================
 * remove_any_xrefs_node_list -- 
 *  Remove from list any occurrences of xref
 * Eg, removing @I2@ from a husband list of a family
 * Returns head of list (which might be different if first node removed)
 *========================================*/
static NODE
remove_any_xrefs_node_list (STRING xref, NODE list)
{
	NODE prev = NULL;
	NODE curr = list;
	NODE rtn = list;

	ASSERT(xref);

	while (curr) {
		if (eqstr(xref, nval(curr))) {
			NODE next = nsibling(curr);
			if (prev)
				nsibling(prev) = next;
			else
				rtn = next;
			nsibling(curr) = NULL;
			free_nodes(curr);
			curr = next;
		}
	}
	return rtn;
}
/*==========================================
 * remove_empty_fam -- Delete family from database
 *  This will call message & fail if there are any
 *  people in the family.
 *========================================*/
BOOLEAN
remove_empty_fam (NODE fam)
{
	STRING key;
	NODE node, husb, wife, chil, rest, refn;
	INT keyint;
	if (!fam) return TRUE;
	split_fam(fam, &refn, &husb, &wife, &chil, &rest);
	if (husb || wife || chil) {
		/* TO DO - This probably should never happen, and maybe could be
		changed to an assertion, 2001/11/08, Perry, but I've not checked
		merge code's call */
		message(_(qShaslnk));
		join_fam(fam, refn, husb, wife, chil, rest);
		return FALSE;
	}
	key = rmvat(nxref(fam));
	keyint = atoi(key + 1);
	addfxref(keyint);
	for (node = refn; node; node = nsibling(node))
		if (nval(node)) remove_refn(nval(node), key);
	remove_fam_cache(key);
	del_in_dbase(key);
	join_fam(fam, refn, husb, wife, chil, rest);
	free_nodes(fam);
	return TRUE;
}
/*=========================================
 * remove_child -- Remove child from family
 *  silent function
 *=======================================*/
BOOLEAN
remove_child (NODE indi, NODE fam)
{
	NODE node, last;

/* Make sure child is in family and remove his/her CHIL line */
	if (!(node = find_node(fam, "CHIL", nxref(indi), &last))) {
		return FALSE;
	}
	if (last)
		nsibling(last) = nsibling(node);
	else
		nchild(fam) = nsibling(node);
	free_node(node);

/* Remove FAMC line from child */
	node = find_node(indi, "FAMC", nxref(fam), &last);
	ASSERT(node && last);
	nsibling(last) = nsibling(node);
	free_node(node);

/* Update database with changed records */
	indi_to_dbase(indi);
	if (num_fam_xrefs(fam) == 0)
		remove_empty_fam(fam);
	else
		fam_to_dbase(fam);
	return TRUE;
}
/*===========================================
 * remove_spouse -- Remove spouse from family
 *  both arguments required
 *  silent function
 *=========================================*/
BOOLEAN
remove_spouse (NODE indi, NODE fam)
{
	NODE node=0, last=0;

/* Remove reference from family */
	node = find_node(fam, "HUSB", nxref(indi), &last);
	if (!node) {
		node = find_node(fam, "WIFE", nxref(indi), &last);
	}
	if (!node)
		return FALSE;

	if (last)
		nsibling(last) = nsibling(node);
	else
		nchild(fam) = nsibling(node);
	free_node(node);
	node = NULL;

/* Remove FAMS line from spouse */
	node = find_node(indi, "FAMS", nxref(fam), &last);
	ASSERT(node && last);
	nsibling(last) = nsibling(node);
	free_node(node);
	node = NULL;

/* Update database with change records */
	indi_to_dbase(indi);

/* Update family (delete if empty) */
	if (num_fam_xrefs(fam) > 0)
		fam_to_dbase(fam);
	else
		remove_empty_fam(fam);

	return TRUE;
}
/*=======================================================
 * num_fam_xrefs -- Find number of person links in family
 *   LOOSEEND -- How about other links in the future???
 *=====================================================*/
INT
num_fam_xrefs (NODE fam)
{
	INT num;
	NODE fref, husb, wife, chil, rest;

	split_fam(fam, &fref, &husb, &wife, &chil, &rest);
	num = length_nodes(husb) + length_nodes(wife) + length_nodes(chil);
	join_fam(fam, fref, husb, wife, chil, rest);
	return num;
}
