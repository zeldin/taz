#include <stdio.h>
#include <time.h>
#include <string.h>
#include "taz.h"
#include "smach.h"
#include "backend.h"

int maxrecurse=100;

extern int optind;
extern char *optarg;

static char *gen_basename(char *fn)
{
  char *r = strrchr(fn, '/');
  if(r != NULL)
    return r+1;
  else
    return fn;
}

int main(int argc, char *argv[])
{
  extern int yyparse(void);
  int i, c, rc = 0;
  int maxpasses = 5;
  int hexout = 0, ihex = 0;

  file_init();
  macro_init();
  smach_init();
  backend_init();
  while((c=getopt(argc, argv, "hsxVI:"))!=EOF)
    switch(c) {
    case 'h':
      fprintf(stderr, "Usage: %s [-h] [-s] [-x] [-V] [-I incdir] sourcefile ...\n",
	      argv[0]);
      break;
    case 'x':
      ihex=1;
      /* FALLTHRU */
    case 's':
      hexout=1;
      break;
    case 'V':
      fprintf(stderr, TARGET" v1.10 by Marcus Comstedt <marcus@mc.pp.se>\n");
      break;
    case 'I':
      add_incdir(optarg);
      break;
    default:
      fprintf(stderr, "Say what?\n");
      exit(1);
    }
  add_incdir("");

  for(i=optind; i<argc; i++) {
    numerrors=0; numlines=0;
    if(process_file(argv[i])) {
      clock_t starttime, stoptime;
      struct symbol *undecided = NULL;
      int passes = maxpasses;
      symbol_init();
      starttime = clock();
      yyparse();
      smach_term();
      symbol_check();
      while(numerrors==0 && (undecided = smach_run()) != NULL && --passes);
      backend_finalize();
      if(undecided != NULL && !passes) {
	fprintf(stderr, "%s line %d: can't find invariant for %s after %d passes\n",
		undecided->defined_file, undecided->defined_line,
		undecided->name, maxpasses);
	numerrors++;
      }
      stoptime = clock();
      symbol_end();
      fprintf(stderr, "Assembly of %s completed in %d ms, %d lines munched, "
	      "total errors %d\n", argv[i], (int)(1000*(stoptime-starttime)/CLOCKS_PER_SEC),
	      numlines, numerrors);
      if(numerrors)
	rc = 1;
      else {
	char *base = gen_basename(argv[i]);
	char *p, *fn = malloc(strlen(base)+5);
	FILE *f;
	if(fn == NULL) {
	  fprintf(stderr, "Fatal: Out of memory!\n");
	  exit(2);
	}
	strcpy(fn, base);
	if((p = strrchr(fn, '.')) != NULL)
	  *p = '\0';
	strcat(fn, (hexout? (ihex?".ihx":".srec"):".bin"));
	if((f = fopen(fn, "wb"))!=NULL) {
	  if(!(hexout? write_section_srec(&builtin_section, f, (ihex?":%02X%04X00":NULL)) :
	       write_section_data(&builtin_section, f))) {
	    fprintf(stderr, "%s: write error\n", fn);
	    rc = 1;
	  }
	  if(ihex)
	    fprintf(f, ":00000001FF\n");
	  fclose(f);
	} else {
	  perror(fn);
	  rc = 1;
	}
	free(fn);
      }
    } else {
      perror(argv[i]);
      backend_end();
      smach_end();
      macro_end();
      file_end();
      return 1;
    }
  }
  backend_end();
  smach_end();
  macro_end();
  file_end();
  return rc;
}

