/* UNIX V7 source code: see /COPYRIGHT or www.tuhs.org for details. */
/* Changes: Copyright (c) 1999 Robert Nordier. All rights reserved. */

#
/*
 * UNIX shell
 *
 * S. R. Bourne
 * Bell Telephone Laboratories
 *
 */

#include	"defs.h"


static STRING	execs();
static void	gsort();
static INT	split();

#define ARGMK	01

INT		errno;
STRING		sysmsg[];

/* fault handling */
#define ENOMEM	12
#define ENOEXEC 8
#define E2BIG	7
#define ENOENT	2
#define ETXTBSY 26



/* service routines for `execute' */

void	initio(iop)
	IOPTR		iop;
{
	REG STRING	ion;
	REG INT		iof, fd;

	IF iop
	) {	iof=iop->iofile;
		ion=mactrim(iop->ioname);
		IF *ion && (flags&noexec)==0
		) {	IF iof&IODOC
			) {	subst(chkopen(ion),(fd=tmpfil()));
				close(fd); fd=chkopen(tmpout); unlink(tmpout);
			} else if ( iof&IOMOV
			) {	IF eq(minus,ion)
				) {	fd = -1;
					close(iof&IOUFD);
				} else if ( (fd=stoi(ion))>=USERIO
				) {	failed(ion,badfile);
				} else {	fd=dup(fd);
				FI
			} else if ( (iof&IOPUT)==0
			) {	fd=chkopen(ion);
			} else if ( flags&rshflg
			) {	failed(ion,restricted);
			} else if ( iof&IOAPP && (fd=open(ion,1))>=0
			) {	lseek(fd, 0L, 2);
			} else {	fd=create(ion);
			FI
			IF fd>=0
			) {	sh_rename(fd,iof&IOUFD);
			FI
		FI
		initio(iop->ionxt);
	FI
}

STRING	getpath(s)
	STRING		s;
{
	REG STRING	path;
	IF any('/',s)
	) {	IF flags&rshflg
		) {	failed(s, restricted);
		} else {	return(nullstr);
		FI
	} else if ( (path = pathnod.namval)==0
	) {	return(defpath);
	} else {	return(cpystak(path));
	FI
}

INT	pathopen(path, name)
	REG STRING	path, name;
{
	REG UFD		f;

	REP path=catpath(path,name);
	PER (f=open(curstak(),0))<0 && path DONE
	return(f);
}

STRING	catpath(path,name)
	REG STRING	path;
	STRING		name;
{
	/* leaves result on top of stack */
	REG STRING	scanp = path,
			argp = locstak();

	WHILE *scanp && *scanp!=COLON DO *argp++ = *scanp++ OD
	IF scanp!=path ) { *argp++='/' FI
	IF *scanp==COLON ) { scanp++ FI
	path=(*scanp ? scanp : 0); scanp=name;
	WHILE (*argp++ = *scanp++) DONE
	return(path);
}

static STRING	xecmsg;
static STRING	*xecenv;

void	execa(at)
	STRING		at[];
{
	REG STRING	path;
	REG STRING	*t = at;

	IF (flags&noexec)==0
	) {	xecmsg=notfound; path=getpath(*t);
		namscan(exname);
		xecenv=sh_setenv();
		WHILE path=execs(path,t) DONE
		failed(*t,xecmsg);
	FI
}

static STRING	execs(ap,t)
	STRING		ap;
	REG STRING	t[];
{
	REG STRING	p, prefix;

	prefix=catpath(ap,t[0]);
	trim(p=curstak());

	sigchk();
	execve(p, &t[0] ,xecenv);
	switch(errno) {
	    case ENOEXEC:
		flags=0;
		comdiv=0; ioset=0;
		clearup(); /* remove open files and for loop junk */
		IF input ) { close(input) FI
		close(output); output=2;
		input=chkopen(p);

		/* set up new args */
		setargs(t);
		longjmp(subshell,1);

	    case ENOMEM:
		failed(p,toobig);

	    case E2BIG:
		failed(p,arglist);

	    case ETXTBSY:
		failed(p,txtbsy);

	    default:
		xecmsg=badexec;
	    case ENOENT:
		return(prefix);
	}
}

/* for processes to be waited for */
#define MAXP 20
static INT	pwlist[MAXP];
static INT	pwc;

postclr()
{
	REG INT		*pw = pwlist;

	WHILE pw <= &pwlist[pwc]
	DO *pw++ = 0 OD
	pwc=0;
}

void	post(pcsid)
	INT		pcsid;
{
	REG INT		*pw = pwlist;

	IF pcsid
	) {	WHILE *pw DO pw++ OD
		IF pwc >= MAXP-1
		) {	pw--;
		} else {	pwc++;
		FI
		*pw = pcsid;
	FI
}

void	await(i)
	INT		i;
{
	INT		rc=0, wx=0;
	INT		w;
	INT		ipwc = pwc;

	post(i);
	WHILE pwc
	DO	REG INT		p;
		REG INT		sig;
		INT		w_hi;

		{
		   REG INT	*pw=pwlist;
		   p=wait(&w);
		   WHILE pw <= &pwlist[ipwc]
		   DO IF *pw==p
		      ) { *pw=0; pwc--;
		      } else { pw++;
		      FI
		   OD
		}

		IF p == -1 ) { continue FI

		w_hi = (w>>8)&LOBYTE;

		IF sig = w&0177
		) {	IF sig == 0177	/* ptrace! return */
			) {	prs("ptrace: ");
				sig = w_hi;
			FI
			IF sysmsg[sig]
			) {	IF i!=p || (flags&prompt)==0 ) { prp(); prn(p); blank() FI
				prs(sysmsg[sig]);
				IF w&0200 ) { prs(coredump) FI
			FI
			newline();
		FI

		IF rc==0
		) {	rc = (sig ? sig|SIGFLG : w_hi);
		FI
		wx |= w;
	OD

	IF wx && flags&errflg
	) {	exitsh(rc);
	FI
	exitval=rc; exitset();
}

BOOL		nosubst;

trim(at)
	STRING		at;
{
	REG STRING	p;
	REG CHAR	c;
	REG CHAR	q=0;

	IF p=at
	) {	WHILE c = *p
		DO *p++=c&STRIP; q |= c OD
	FI
	nosubst=q&QUOTE;
}

STRING	mactrim(s)
	STRING		s;
{
	REG STRING	t=macro(s);
	trim(t);
	return(t);
}

STRING	*scan(argn)
	INT		argn;
{
	REG ARGPTR	argp = (ARGPTR)(Rcheat(gchain)&~ARGMK);
	REG STRING	*comargn, *comargm;

	comargn=(STRING *)getstak(BYTESPERWORD*argn+BYTESPERWORD); comargm = comargn += argn; *comargn = ENDARGS;

	WHILE argp
	DO	*--comargn = argp->argval;
		IF argp = argp->argnxt
		) { trim(*comargn);
		FI
		IF argp==0 || Rcheat(argp)&ARGMK
		) {	gsort(comargn,comargm);
			comargm = comargn;
		FI
		/* Lcheat(argp) &= ~ARGMK; */
		argp = (ARGPTR)(Rcheat(argp)&~ARGMK);
	OD
	return(comargn);
}

static void	gsort(from,to)
	STRING		from[], to[];
{
	INT		k, m, n;
	REG INT		i, j;

	IF (n=to-from)<=1 ) { return FI

	FOR j=1; j<=n; j*=2 DONE

	FOR m=2*j-1; m/=2;
	DO  k=n-m;
	    FOR j=0; j<k; j++
	    DO	FOR i=j; i>=0; i-=m
		DO  REG STRING *fromi; fromi = &from[i];
		    IF cf(fromi[m],fromi[0])>0
		    ) { break;
		    } else { STRING s; s=fromi[m]; fromi[m]=fromi[0]; fromi[0]=s;
		    FI
		OD
	    OD
	OD
}

/* Argument list generation */

INT	getarg(ac)
	COMPTR		ac;
{
	REG ARGPTR	argp;
	REG INT		count=0;
	REG COMPTR	c;

	IF c=ac
	) {	argp=c->comarg;
		WHILE argp
		DO	count += split(macro(argp->argval));
			argp=argp->argnxt;
		OD
	FI
	return(count);
}

static INT	split(s)
	REG STRING	s;
{
	REG STRING	argp;
	REG INT		c;
	INT		count=0;

	for(;;) {
		sigchk(); argp=locstak()+BYTESPERWORD;
		WHILE (c = *s++, !any(c,ifsnod.namval) && c)
		DO *argp++ = c OD
		IF argp==staktop+BYTESPERWORD
		) {	IF c
			) {	continue;
			} else {	return(count);
			FI
		} else if (c==0
		) {	s--;
		FI
		IF c=expand(((ARGPTR)(argp=endstak(argp)))->argval,0)
		) {	count += c;
		} else {	/* assign(&fngnod, argp->argval); */
			makearg(argp); count++;
		FI
		Lcheat(gchain) |= ARGMK;
	}
}
