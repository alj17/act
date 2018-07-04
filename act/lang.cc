/*************************************************************************
 *
 *  Copyright (c) 2018 Rajit Manohar
 *  All Rights Reserved
 *
 **************************************************************************
 */
#include <act/types.h>
#include <act/lang.h>
#include <act/inst.h>
#include <act/prs.h>
#include "qops.h"

act_size_spec_t *act_expand_size (act_size_spec_t *sz, ActNamespace *ns, Scope *s);

static ActId *expand_var_read (ActId *id, ActNamespace *ns, Scope *s)
{
  ActId *idtmp;
  Expr *etmp;
  
  if (!id) return NULL;
  
  idtmp = id->Expand (ns, s);
  etmp = idtmp->Eval (ns, s);
  Assert (etmp->type == E_VAR, "Hmm");
  delete idtmp;
  idtmp = (ActId *)etmp->u.e.l;
  /* check that type is a bool */
  FREE (etmp);
  return idtmp;
}

act_prs *prs_expand (act_prs *p, ActNamespace *ns, Scope *s)
{
  act_prs *ret;

  if (!p) return NULL;

  NEW (ret, act_prs);

  ret->vdd = expand_var_read (p->vdd, ns, s);
  ret->gnd = expand_var_read (p->gnd, ns, s);
  ret->psc = expand_var_read (p->psc, ns, s);
  ret->nsc = expand_var_read (p->nsc, ns, s);
  ret->p = prs_expand (p->p, ns, s);

  return ret;
}


act_prs_expr_t *prs_expr_expand (act_prs_expr_t *p, ActNamespace *ns, Scope *s)
{
  int pick;
  
  act_prs_expr_t *ret = NULL;
  if (!p) return NULL;

  NEW (ret, act_prs_expr_t);
  ret->type = p->type;
  switch (p->type) {
  case ACT_PRS_EXPR_AND:
  case ACT_PRS_EXPR_OR:
    ret->u.e.l = prs_expr_expand (p->u.e.l, ns, s);
    ret->u.e.r = prs_expr_expand (p->u.e.r, ns, s);
    ret->u.e.pchg = prs_expr_expand (p->u.e.pchg, ns, s);
    ret->u.e.pchg_type = p->u.e.pchg_type;

    pick = -1;

    if (ret->u.e.l->type == ACT_PRS_EXPR_TRUE) {
      /* return ret->u.e.r; any pchg is skipped */
      if (p->type == ACT_PRS_EXPR_AND) {
	pick = 0; /* RIGHT */
      }
      else {
	pick = 1; /* LEFT */
      }
    }
    else if (ret->u.e.l->type == ACT_PRS_EXPR_FALSE) {
	if (p->type == ACT_PRS_EXPR_AND) {
	  pick = 1; /* LEFT */
	}
	else {
	  pick = 0; /* RIGHT */
	}
    }
    else if (ret->u.e.r->type == ACT_PRS_EXPR_TRUE) {
      if (p->type == ACT_PRS_EXPR_AND) {
	pick = 1; /* LEFT */
      }
      else {
	pick = 0; /* RIGHT */
      }
    }
    else if (ret->u.e.r->type == ACT_PRS_EXPR_FALSE) {
      if (p->type == ACT_PRS_EXPR_AND) {
	pick = 0; /* RIGHT */
      }
      else {
	pick = 1; /* LEFT */
      }
    }
    if (pick == 0) {
      act_prs_expr_t *t;
      /* RIGHT */
      act_free_a_prs_expr (ret->u.e.l);
      act_free_a_prs_expr (ret->u.e.pchg);
      t = ret->u.e.r;
      FREE (ret);
      ret = t;
    }
    else if (pick == 1) {
      act_prs_expr_t *t;
      /* LEFT */
      act_free_a_prs_expr (ret->u.e.r);
      act_free_a_prs_expr (ret->u.e.pchg);
      t = ret->u.e.l;
      FREE (ret);
      ret = t;
    }
    break;

  case ACT_PRS_EXPR_NOT:
    ret->u.e.l = prs_expr_expand (p->u.e.l, ns, s);
    ret->u.e.r = NULL;
    if (ret->u.e.l->type == ACT_PRS_EXPR_FALSE) {
      act_prs_expr_t *t;
      ret->u.e.l->type = ACT_PRS_EXPR_TRUE;
      t = ret->u.e.l;
      FREE (ret);
      ret = t;
    }
    else if (ret->u.e.l->type == ACT_PRS_EXPR_TRUE) {
      act_prs_expr_t *t;
      ret->u.e.l->type = ACT_PRS_EXPR_FALSE;
      t = ret->u.e.l;
      FREE (ret);
      ret = t;
    }
    break;

  case ACT_PRS_EXPR_LABEL:
    ret->u.l.label = p->u.l.label;
    break;

  case ACT_PRS_EXPR_VAR:
    ret->u.v.sz = act_expand_size (p->u.v.sz, ns, s);
    ret->u.v.id = expand_var_read (p->u.v.id, ns, s);
    break;

  case ACT_PRS_EXPR_ANDLOOP:
  case ACT_PRS_EXPR_ORLOOP:
    Assert (s->Add (p->u.loop.id, TypeFactory::Factory()->NewPInt()),
	    "Huh?");
    {
      ValueIdx *vx;
      unsigned int ilo, ihi, i;
      Expr *etmp;

      if (p->u.loop.lo) {
	etmp = expr_expand (p->u.loop.lo, ns, s);
	if (etmp->type != E_INT) {
	  act_error_ctxt (stderr);
	  fprintf (stderr, "Expanding loop range in prs body\n  expr: ");
	  print_expr (stderr, p->u.loop.lo);
	  fprintf (stderr, "\nNot a constant int\n");
	  exit (1);
	}
	ilo = etmp->u.v;
	FREE (etmp);
      }
      else {
	ilo = 0;
      }

      etmp = expr_expand (p->u.loop.hi, ns, s);
      if (etmp->type != E_INT) {
	act_error_ctxt (stderr);
	fprintf (stderr, "Expanding loop range in prs body\n  expr: ");
	print_expr (stderr, p->u.loop.hi);
	fprintf (stderr, "\nNot a constant int\n");
	exit (1);
      }
      if (p->u.loop.lo) {
	ihi = etmp->u.v-1;
      }
      else {
	ihi = etmp->u.v;
      }
      FREE (etmp);

      vx = s->LookupVal (p->u.loop.id);
      vx->init = 1;
      vx->u.idx = s->AllocPInt();

      if (ihi < ilo) {
	/* empty */
	if (p->type == ACT_PRS_EXPR_ANDLOOP) {
	  ret->type = ACT_PRS_EXPR_TRUE;
	}
	else {
	  ret->type = ACT_PRS_EXPR_FALSE;
	}
      }
      else {
	FREE (ret);
	ret = NULL;
	for (i=ilo; i <= ihi; i++) {
	  act_prs_expr_t *at;
	  at = prs_expr_expand (p->u.loop.e, ns, s);

	  if (!ret) {
	    ret = at;
	    if (ret->type == ACT_PRS_EXPR_TRUE) {
	      if (p->type == ACT_PRS_EXPR_ANDLOOP) {
		act_free_a_prs_expr (ret);
		ret = NULL;
	      }
	      else {
		break;
	      }
	    }
	    else if (ret->type == ACT_PRS_EXPR_FALSE) {
	      if (p->type == ACT_PRS_EXPR_ANDLOOP) {
		break;
	      }
	      else {
		act_free_a_prs_expr (ret);
		ret = NULL;
	      }
	    }
	  }
	  else if (at->type == ACT_PRS_EXPR_TRUE) {
	    if (p->type == ACT_PRS_EXPR_ANDLOOP) {
	      /* nothing */
	    }
	    else {
	      /* we're done! */
	      act_free_a_prs_expr (ret);
	      ret = at;
	      break;
	    }
	  }
	  else if (at->type == ACT_PRS_EXPR_FALSE) {
	    if (p->type == ACT_PRS_EXPR_ANDLOOP) {
	      /* we're done */
	      act_free_a_prs_expr (ret);
	      ret = at;
	      break;
	    }
	    else {
	      /* nothing */
	    }
	  }
	}
	if (ret == NULL) {
	  NEW (ret, act_prs_expr_t);
	  if (p->type == ACT_PRS_EXPR_ANDLOOP) {
	    ret->type = ACT_PRS_EXPR_TRUE;
	  }
	  else {
	    ret->type = ACT_PRS_EXPR_FALSE;
	  }
	}
      }
      s->DeallocPInt (vx->u.idx, 1);
    }
    s->Del (p->u.loop.id);
    break;

  case ACT_PRS_EXPR_TRUE:
  case ACT_PRS_EXPR_FALSE:
    break;
    
  default:
    Assert (0, "Eh?");
    break;
  }
  
  
  return ret;
}

act_attr_t *attr_expand (act_attr_t *a, ActNamespace *ns, Scope *s)
{
  act_attr_t *hd = NULL, *tl = NULL, *tmp;

  while (a) {
    NEW (tmp, act_attr_t);
    tmp->attr = a->attr;
    tmp->e = expr_expand (a->e, ns, s);
    if (tmp->e->type != E_INT || tmp->e->type != E_REAL ||
	tmp->e->type != E_TRUE || tmp->e->type != E_FALSE) {
      act_error_ctxt (stderr);
      fprintf (stderr, "attribute %s is assigned a non-constant expression\n", tmp->attr);
      fprintf (stderr, "  expr: ");
      print_expr (stderr, a->e);
      fprintf (stderr, "\n");

      exit (1);
    }
    q_ins (hd, tl, tmp);
  }
  return hd;
}

act_size_spec_t *act_expand_size (act_size_spec_t *sz, ActNamespace *ns, Scope *s)
{
  act_size_spec_t *ret;
  if (!sz) return NULL;

  NEW (ret, act_size_spec_t);
  if (sz->w) {
    ret->w = expr_expand (sz->w, ns, s);
    if (ret->w->type != E_INT && ret->w->type != E_REAL) {
      act_error_ctxt (stderr);
      fprintf (stderr, "Size expression is not of type pint or preal\n");
      fprintf (stderr, " expr: ");
      print_expr (stderr, sz->w);
      fprintf (stderr, "\n");
      exit (1);
    }
  }
  else {
    ret->w = NULL;
  }
  if (sz->l) {
    ret->l = expr_expand (sz->l, ns, s);
    if (ret->l->type != E_INT && ret->l->type != E_REAL) {
      act_error_ctxt (stderr);
      fprintf (stderr, "Size expression is not of type pint or preal\n");
      fprintf (stderr, " expr: ");
      print_expr (stderr, sz->l);
      fprintf (stderr, "\n");
      exit (1);
    }
  }
  else {
    ret->l = NULL;
  }
  ret->flavor = sz->flavor;
  ret->subflavor = sz->subflavor;
  
  return ret;
}

act_prs_lang_t *prs_expand (act_prs_lang_t *p, ActNamespace *ns, Scope *s)
{
  act_prs_lang_t *hd = NULL;
  act_prs_lang_t *tl = NULL;
  act_prs_lang_t *tmp;
  ActId *idtmp;
  Expr *etmp;

  while (p) {
    /* expand one prs */
    NEW (tmp, act_prs_lang_t);
    tmp->type = p->type;
    tmp->next = NULL;
    switch (p->type) {
    case ACT_PRS_RULE:
      tmp->u.one.attr = attr_expand (p->u.one.attr, ns, s);
      tmp->u.one.e = prs_expr_expand (p->u.one.e, ns, s);

      if (p->u.one.label == 0) {
	idtmp = p->u.one.id->Expand (ns, s);
	etmp = idtmp->Eval (ns, s);
	Assert (etmp->type == E_VAR, "Hmm");
	tmp->u.one.id = (ActId *)etmp->u.e.l;
	FREE (etmp);
      }
      else {
	/* it is a char* */
	tmp->u.one.id = p->u.one.id;
      }
      tmp->u.one.arrow_type = p->u.one.arrow_type;
      tmp->u.one.dir = p->u.one.dir;
      tmp->u.one.label = p->u.one.label;
      break;
      
    case ACT_PRS_GATE:
      tmp->u.p.attr = attr_expand (p->u.p.attr, ns, s);
      idtmp = p->u.p.s->Expand (ns, s);
      etmp = idtmp->Eval (ns, s);
      Assert (etmp->type == E_VAR, "Hm");
      tmp->u.p.s = (ActId *)etmp->u.e.l;
      FREE (etmp);

      idtmp = p->u.p.d->Expand (ns, s);
      etmp = idtmp->Eval (ns, s);
      Assert (etmp->type == E_VAR, "Hm");
      tmp->u.p.d = (ActId *)etmp->u.e.l;
      FREE (etmp);

      if (p->u.p.g) {
	idtmp = p->u.p.g->Expand (ns, s);
	etmp = idtmp->Eval (ns, s);
	Assert (etmp->type == E_VAR, "Hm");
	tmp->u.p.g = (ActId *)etmp->u.e.l;
	FREE (etmp);
      }
      else {
	tmp->u.p.g = NULL;
      }

      if (p->u.p._g) {
	idtmp = p->u.p._g->Expand (ns, s);
	etmp = idtmp->Eval (ns, s);
	Assert (etmp->type == E_VAR, "Hm");
	tmp->u.p._g = (ActId *)etmp->u.e.l;
	FREE (etmp);
      }
      else {
	tmp->u.p._g = NULL;
      }
      tmp->u.p.sz = act_expand_size (p->u.p.sz, ns, s);
      break;
      
    case ACT_PRS_LOOP:
      Assert (s->Add (p->u.l.id, TypeFactory::Factory()->NewPInt()),
	      "Should have been caught earlier");
      {
	ValueIdx *vx;
	unsigned int ilo, ihi, i;
	act_prs_lang_t *px, *pxrethd, *pxrettl;
	Expr *etmp;
	
	if (p->u.l.lo) {
	  etmp = expr_expand (p->u.l.lo, ns, s);
	  if (etmp->type != E_INT) {
	    act_error_ctxt (stderr);
	    fprintf (stderr, "Expanding loop range in prs body\n  expr: ");
	    print_expr (stderr, p->u.l.lo);
	    fprintf (stderr, "\nNot a constant int\n");
	    exit (1);
	  }
	  ilo = etmp->u.v;
	  FREE (etmp);
	}
	else {
	  ilo = 0;
	}
	etmp = expr_expand (p->u.l.hi, ns, s);
	if (etmp->type != E_INT) {
	  act_error_ctxt (stderr);
	  fprintf (stderr, "Expanding loop range in prs body\n  expr: ");
	  print_expr (stderr, p->u.l.hi);
	  fprintf (stderr, "\nNot a constant int\n");
	  exit (1);
	}
	if (p->u.l.lo) {
	  ihi = etmp->u.v-1;
	}
	else {
	  ihi = etmp->u.v;
	}
	FREE (etmp);

	vx = s->LookupVal (p->u.l.id);
	vx->init = 1;
	vx->u.idx = s->AllocPInt();

	pxrethd = NULL;
	pxrettl = NULL;
	for (i = ilo; i <= ihi; i++) {
	  s->setPInt (vx->u.idx, i);
	  px = prs_expand (p->u.l.p, ns, s);
	  if (!pxrethd) {
	    pxrethd = px;
	    pxrettl = pxrethd;
	    while (pxrettl->next) {
	      pxrettl = pxrettl->next;
	    }
	  }
	  else {
	    pxrettl->next = px;
	    while (pxrettl->next) {
	      pxrettl = pxrettl->next;
	    }
	  }
	}
	FREE (tmp);
	tmp = pxrethd;
	s->DeallocPInt (vx->u.idx, 1);
      }
      s->Del (p->u.l.id);
      break;
      
    case ACT_PRS_TREE:
      etmp = expr_expand (p->u.l.lo, ns, s);
      if (etmp->type != E_INT) {
	act_error_ctxt (stderr);
	fprintf (stderr, "tree<> expression is not of type pint\n");
	fprintf (stderr, " expr: ");
	print_expr (stderr, p->u.l.lo);
	fprintf (stderr, "\n");
	exit (1);
      }
      tmp->u.l.lo = etmp;
      tmp->u.l.hi = NULL;
      tmp->u.l.id = NULL;
      tmp->u.l.p = prs_expand (p->u.l.p, ns, s);
      break;
      
    case ACT_PRS_SUBCKT:
      tmp->u.l.id = p->u.l.id;
      tmp->u.l.lo = NULL;
      tmp->u.l.hi = NULL;
      tmp->u.l.p = prs_expand (p->u.l.p, ns, s);
      break;
      
    default:
      Assert (0, "Should not be here");
      break;
    }
    if (tmp) {
      q_ins (hd, tl, tmp);
      while (tl->next) {
	tl = tl->next;
      }
    }
    p = q_step (p);
  }
  return hd;
}

act_chp *chp_expand (act_chp *c, ActNamespace *ns, Scope *s)
{
  /* s->u must exist */
  return c;
}

act_chp_lang_t *chp_expand (act_chp_lang_t *c, ActNamespace *ns, Scope *s)
{
  /* s->u must exist */
  return c;
}



