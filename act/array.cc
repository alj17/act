/*************************************************************************
 *
 *  Copyright (c) 2018 Rajit Manohar
 *  All Rights Reserved
 *
 **************************************************************************
 */
#include <act/types.h>
#include <act/inst.h>
#include "list.h"


/*------------------------------------------------------------------------
 * Basic constructor
 *------------------------------------------------------------------------
 */
Array::Array()
{
  r = NULL;
  dims = 0;
  next = NULL;
  deref = 0;
  expanded = 0;
}

/*------------------------------------------------------------------------
 * Constructor: [e..f] single dimensional array
 *------------------------------------------------------------------------
 */
Array::Array (Expr *e, Expr *f)
{
  dims = 1;
  NEW (r, struct range);

  if (f == NULL) {
    r->u.ue.lo = NULL;
    r->u.ue.hi = e;
    deref = 1;
  }
  else {
    r->u.ue.lo = e;
    r->u.ue.hi = f;
    deref = 0;
  }
  expanded = 0;
  range_sz = -1;
  next = NULL;
}

/*------------------------------------------------------------------------
 *  Destructor
 *------------------------------------------------------------------------
 */
Array::~Array ()
{
  if (next) {
    delete next;
  }
  FREE (r);
}


/*------------------------------------------------------------------------
 *
 *  Array::Clone --
 *
 *   Deep copy of array
 *
 *------------------------------------------------------------------------
 */
Array *Array::Clone ()
{
  Array *ret;

  ret = new Array();

  ret->deref = deref;
  ret->expanded = expanded;
  if (next) {
    ret->next = next->Clone ();
  }
  else {
    ret->next = NULL;
  }
  ret->dims = dims;

  if (dims > 0) {
    MALLOC (ret->r, struct range, dims);
    for (int i= 0; i < dims; i++) {
      ret->r[i] = r[i];
    }
  }
  return ret;
}


/*------------------------------------------------------------------------
 * Compare two arrays
 *   dims have to be compatible
 *   strict = 0 : just check ranges have the same sizes
 *   strict = 1 : the ranges must be the same
 *   strict = -1 : ranges have same sizes, except for the first one
 *------------------------------------------------------------------------
 */
int Array::isEqual (Array *a, int strict) 
{
  struct range *r1, *r2;
  int i;

  if (!isDimCompatible (a)) return 0;

  r1 = r;
  r2 = a->r;

  if (!expanded) {
    /* no distinction between strict and non-strict */
    for (i=0; i < dims; i++) {
      if ((r1[i].u.ue.lo && !r2[i].u.ue.lo) || (!r1[i].u.ue.lo && r2[i].u.ue.lo))
	return 0;
      if (r1[i].u.ue.lo && !expr_equal (r1[i].u.ue.lo, r2[i].u.ue.lo)) return 0;
      if (r1[i].u.ue.hi && !expr_equal (r1[i].u.ue.hi, r2[i].u.ue.hi)) return 0;
    }
  }
  else {
    for (i=(strict == -1 ? 1 : 0); i < dims; i++) {
      if (strict == 1) {
	if ((r1[i].u.ex.lo != r2[i].u.ex.lo) || (r1[i].u.ex.hi != r2[i].u.ex.hi))
	  return 0;
      }
      else {
	if ((r1[i].u.ex.hi - r1[i].u.ex.lo) != (r2[i].u.ex.hi - r2[i].u.ex.lo))
	  return 0;
      }
    }
  }
  if (next) {
    return next->isEqual (a->next, strict);
  }
  return 1;
}

/*------------------------------------------------------------------------
 * Check if two arrays are compatible
 *------------------------------------------------------------------------
 */
int Array::isDimCompatible (Array *a)
{
  struct range *r1, *r2;
  int i;

  if (dims != a->dims) return 0;
  if (deref != a->deref) return 0;
  if (isSparse() != a->isSparse()) return 0;
  
  return 1;
}


/*------------------------------------------------------------------------
 *  Increase size of this array by appending dimensions from "a"
 *------------------------------------------------------------------------
 */
void Array::Concat (Array *a)
{
  int i;

  Assert (isSparse() == 0, "Array::Concat() only supported for dense arrays");
  Assert (a->isSparse() == 0, "Array::Concat() only works for dense arrays");
  Assert (a->isExpanded () == expanded, "Array::Concat() must have same expanded state");
  
  dims += a->dims;
  
  REALLOC (r, struct range, dims);
  
  for (i=0; i < a->dims; i++) {
    r[dims - a->dims + i] = a->r[i];
  }

  /* if any part is not a deref, it is a subrange */
  if (a->deref == 0) {
    deref = 0;
  }

  range_sz = -1;
}


/*------------------------------------------------------------------------
 *  Given an array, this returns the linear offset within the array of
 *  the deref "a".
 *
 *   -1 if "a" is not within the bounds of the current range
 *------------------------------------------------------------------------
 */
int Array::in_range (Array *a)
{
  int sz;
  int offset;
  
  /* expanded only */
  /* this is true, but in_range is a private function 
    Assert (a->isDeref (), "Hmmm");
  */

  /* compute the size */
  if (range_sz == -1) {
    size();
  }

  sz = range_sz;
  offset = 0;
  
  for (int i=0; i < dims; i++) {
    int d;

    /*Assert (a->r[i].u.ex.lo == a->r[i].u.ex.hi, "What?");*/
    d = a->r[i].u.ex.lo;
    if (r[i].u.ex.lo > d || r[i].u.ex.hi < d) {
      return -1;
    }
    sz = sz / (r[i].u.ex.hi - r[i].u.ex.lo + 1);
    offset = offset + (d - r[i].u.ex.lo)*sz;

    d = a->r[i].u.ex.hi;
    if (r[i].u.ex.lo > d || r[i].u.ex.hi < d) {
      return -1;
    }
    
    Assert ((i != dims-1) || (sz == 1), "Wait a sec");
  }
  return offset;
}


/*------------------------------------------------------------------------
 *  Finds offset of "a" within the current array. -1 if not found.
 *  This uses in_range as a helper function.
 *------------------------------------------------------------------------
 */
int Array::Offset (Array *a)
{
  int offset;
  
  /* expanded only */
  Assert (expanded && a->isExpanded(), "Hmm...");

  /*Assert (a->isDeref (), "Hmm...");*/

  offset = in_range (a);
  if (offset != -1) {
    return offset;
  }
  else {
    offset = next->Offset (a);
    if (offset == -1) {
      return -1;
    }
    else {
      return range_sz + offset;
    }
  }
}


/*------------------------------------------------------------------------
 *  "a" is a deref
 *  Returns 1 if "a" is a valid de-reference for the array, 0 otherwise.
 *------------------------------------------------------------------------
 */
int Array::Validate (Array *a)
{
  if (!expanded || !a->isExpanded()) {
    fatal_error ("should only be called for expanded arrays");
  }
  if (!a->isDeref()) {
    act_error_ctxt (stderr);
    fprintf (stderr, " array: ");
    a->Print (stderr);
    fatal_error ("\nShould be a de-reference!");
  }
  Assert (dims == a->nDims(), "dimensions don't match!");

  int i, d;

  for (i=0; i < dims; i++) {
    d = a->r[i].u.ex.hi + 1;
    if (r[i].u.ex.lo > d || r[i].u.ex.hi < d) {
      if (next) {
	return next->Validate (a);
      }
      else {
	return 0;
      }
    }
  }
  return 1;
}

/*------------------------------------------------------------------------
 * Print out array
 *------------------------------------------------------------------------
 */
void Array::Print (FILE *fp)
{
  Array *pr;
  if (next) {
    fprintf (fp, "[ ");
  }
  pr = this;
  while (pr) {
    fprintf (fp, "[");
    for (int i=0; i < pr->dims; i++) {
      if (!expanded) {
	if (pr->r[i].u.ue.lo == NULL) {
	  print_expr (fp, pr->r[i].u.ue.hi);
	}
	else {
	  print_expr (fp, pr->r[i].u.ue.lo);
	  fprintf (fp, "..");
	  print_expr (fp, pr->r[i].u.ue.hi);
	}
      }
      else {
	if (pr->r[i].u.ex.lo == 0) {
	  fprintf (fp, "%d", pr->r[i].u.ex.hi+1);
	}
	else {
	  fprintf (fp, "%d..%d", pr->r[i].u.ex.lo, pr->r[i].u.ex.hi);
	}
      }
      if (i < pr->dims-1) {
	fprintf (fp, ",");
      }
    }
    fprintf (fp, "]");
    pr = pr->next;
  }
  if (this->next) {
    fprintf (fp, " ]");
  }
}

/*------------------------------------------------------------------------
 *  For arrays, counts the number of dimensions specified as [a..b]
 *  as opposed to [a]
 *------------------------------------------------------------------------
 */
int Array::effDims()
{
  int i;
  int count = 0;

  Assert (!expanded, "Not applicable to expanded arrays!");

  if (isSparse()) return nDims ();

  count = 0;
  for (i=0; i < dims; i++) {
    if (r[i].u.ue.lo == NULL) {
      /* skip */
    }
    else {
      count++;
    }
  }
  return count;
}


/*
 * @return the range of the d'th dimension of the array
 */
int Array::range_size(int d)
{
  int i;
  int count = 1;
  struct Array *a;

  Assert (expanded, "Only applicable to expanded arrays");
  Assert (0 <= d && d < dims, "Invalid dimension");
  Assert (!isSparse(), "Only applicable to dense arrays");

  return r[d].u.ex.hi - r[d].u.ex.lo + 1;
}

/*
 * Update range size
 */
void Array::update_range (int d, int lo, int hi)
{
  Assert (expanded, "Huh?");
  Assert (0 <= d && d < dims, "Invalid dimension");
  Assert (!isSparse(), "Only applicable to dense arrays");
  r[d].u.ex.lo = lo;
  r[d].u.ex.hi = hi;
  range_sz = -1;
}

/*
 * @return number of elements in the array
 */
int Array::size()
{
  int i;
  int count = 1;
  struct Array *a;

  Assert (expanded, "Only applicable to expanded arrays");

  if (range_sz != -1) {
    count = range_sz;
  }
  else {
    for (i=0; i < dims; i++) {
      if (r[i].u.ex.hi < r[i].u.ex.lo) {
	count = 0;
	break;
      }
      else {
	count = count*(r[i].u.ex.hi-r[i].u.ex.lo+1);
      }
    }
    range_sz = count;
  }
  if (next) {
    count += next->size();
  }
  return count;
}


/*------------------------------------------------------------------------
 *
 *  Array::Expand --
 *
 *   Return an expanded array
 *
 *------------------------------------------------------------------------
 */
Array *Array::Expand (ActNamespace *ns, Scope *s, int is_ref)
{
  Array *ret;
  
  if (expanded) {
    /* eh, why am I here anyway */
    act_error_ctxt (stderr);
    warning ("Not sure why Array::Expand() was called");
    return this;
  }

  ret = new Array();
  ret->expanded = 1;
  if (deref) {
    ret->deref = 1;
  }
  ret->dims = dims;
  Assert (dims > 0, "What on earth is going on...");
  MALLOC (ret->r, struct range, dims);

  int i;

  for (i=0; i < dims; i++) {
    Expr *hval, *lval;

    Assert (r[i].u.ue.hi, "Invalid array range");
    hval = expr_expand (r[i].u.ue.hi, ns, s);

#if 0
    fprintf (stderr, "expr: ");
    print_expr (stderr, r[i].u.ue.hi);
    fprintf (stderr, " -> ");
    print_expr(stderr, hval);
    fprintf (stderr, "\n");
#endif
    
    if (hval->type != E_INT) {
      act_error_ctxt (stderr);
      fatal_error ("Array range value is a non-integer/non-constant value");
    }
    if (r[i].u.ue.lo) {
      lval = expr_expand (r[i].u.ue.lo, ns, s);
      if (lval->type != E_INT) {
	act_error_ctxt (stderr);
	fatal_error ("Array range value is a non-integer/non-constant value");
      }
      ret->r[i].u.ex.lo = lval->u.v;
      ret->r[i].u.ex.hi = hval->u.v;
      FREE (lval);
    }
    else {
      if (is_ref) {
	ret->r[i].u.ex.lo = hval->u.v;
	ret->r[i].u.ex.hi = hval->u.v;
      }
      else {
	ret->r[i].u.ex.lo = 0;
	ret->r[i].u.ex.hi = hval->u.v - 1;
      }
    }
    FREE (hval);
  }

  if (next) {
    ret->next = next->Expand (ns, s);
  }

  return ret;
}



/*------------------------------------------------------------------------
 *
 *  Array::stepper --
 *
 *   Returns something that can be used to step the array
 *
 *------------------------------------------------------------------------
 */
Arraystep *Array::stepper ()
{
  Array *a;

  if (!expanded) {
    fatal_error ("Array::stepper() called for an unexpanded array");
  }
  return new Arraystep (this);
}


/*------------------------------------------------------------------------
 * Arraystep functions
 *------------------------------------------------------------------------
 */
Arraystep::Arraystep (Array *a, int _is_subrange)
{
  base = a;
  is_subrange = _is_subrange;
  
  MALLOC (deref, int, base->dims);
  for (int i = 0; i < base->dims; i++) {
    if (is_subrange) {
      deref[i] = base->r[i].u.ex.lo;
    }
    else {
      deref[i] = base->r[i].u.ex.hi + 1;
    }
  }
  idx = 0;
}

Arraystep::~Arraystep()
{
  FREE (deref);
}

/*------------------------------------------------------------------------
 *
 *  Step the array index
 *
 *------------------------------------------------------------------------
 */
void Arraystep::step()
{
  if (!base) return;
  
  for (int i = base->dims - 1; i >= 0; i--) {
    deref[i]++;
    if (deref[i] <= base->r[i].u.ex.hi) {
      /* we're done */
      idx++;
      return;
    }
    /* ok, we've wrapped. need to increase the next index */
    deref[i] = base->r[i].u.ex.lo;
  }

  /* we're here, we've overflowed this range */
  base = base->next;
  if (!base) {
    /* we're done! */
    idx = -1;
    return;
  }
  
  /* set the index to the lowest one in the next range */
  for (int i = 0; i < base->dims; i++) {
    deref[i] = base->r[i].u.ex.lo;
  }
  idx++;
}

/*------------------------------------------------------------------------
 *
 *  Am I at the end?
 *
 *------------------------------------------------------------------------
 */
int Arraystep::isend()
{
  if (base == NULL) return 1;
  else return 0;
}


/*------------------------------------------------------------------------
 * Step through an AExpr. The assumption is that the AExpr is already
 * expanded
 *------------------------------------------------------------------------
 */
AExprstep::AExprstep (AExpr *a)
{
  stack = list_new ();
  cur = a;
  idx = -1;
  type = 0;
  step();
}


AExprstep::~AExprstep ()
{
  list_free (stack);
}

void AExprstep::step()
{
  switch (type) {
  case 0:
  case 1:
    /* nothing to see here, need to continue traversing the AExpr */
    break;
  case 2:
    /* check if we are out of identifiers */
    if (u.id.a && !u.id.a->isend()) {
      u.id.a->step();
      idx++;
      return;
    }
    if (u.id.a) {
      /* finished with the stepper */
      delete u.id.a;
    }
    break;
  case 3:
    /* check if we are out of values */
    if (u.vx.offset < u.vx.max) {
      idx++;
      u.vx.offset++;
      return;
    }
    break;
  default:
    fatal_error ("This is not possible");
    return;
  }

  type = 0;

  if (!cur) {
    if (stack_isempty (stack)) {
      return;
    }
    cur = (AExpr *) stack_pop (stack);
  }

  while (cur) {
    switch (cur->t) {
    case AExpr::EXPR:
      {
	Expr *xe = (Expr *) cur->l;
	Assert (xe->type == E_VAR || expr_is_a_const (xe) || xe->type == E_ARRAY, "What?");
	if (expr_is_a_const (xe)) {
	  /* return the constant! */
	  type = 1;
	  u.const_expr = xe;
	  /* now advance the state */
	}
	else if (xe->type == E_VAR) {
	  /* now we need to figure out what this ID actually is */
	  InstType *it;
	  type = 2;
	  u.id.act_id = (ActId *)xe->u.e.l;
	  u.id.a = NULL;
	  it = (InstType *)xe->u.e.r;

	  if (u.id.act_id->arrayInfo() && u.id.act_id->arrayInfo()->isDeref()) {
	    /* single deref, we're done */
	  }
	  else {
	    /* array slice, we have to step through each of them */
	    u.id.a = u.id.act_id->arrayInfo()->stepper();
	  }
	}
	else if (xe->type == E_ARRAY) {
	  /* array slice */
	  type = 3;
	  u.vx.vx = (ValueIdx *) xe->u.e.l;
	  u.vx.s = (Scope *) xe->u.e.r;
	  u.vx.offset = 0;
	  u.vx.max = u.vx.vx->t->arrayInfo()->size()-1;
	}
	else {
	  Assert (0," Should not be here ");
	}
	idx++;
	cur = (AExpr *) stack_pop (stack);
	return;
      }
      break;

    case AExpr::CONCAT:
    case AExpr::COMMA:
      if (cur->r) {
	stack_push (stack, cur->r);
      }
      cur = cur->l;
      break;
    default:
      fatal_error ("Huh");
      break;
    }
  }
}

int AExprstep::isend()
{
  if (type == 0 && !cur && stack_isempty (stack)) {
    return 1;
  }
  return 0;
}
