#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include "taz.h"
#include "smach.h"
#include "backend.h"

int maxrecurse=100;

int main(int argc, char *argv[])
{
  extern int yyparse(void);
  int i, c;

  file_init();
  macro_init();
  smach_init();
  backend_init();
  while((c=getopt(argc, argv, "I:"))!=EOF)
    switch(c) {
    case 'I':
      add_incdir(optarg);
      break;
    default:
      fprintf(stderr, "Say what?\n");
      exit(1);
    }
  add_incdir("");

  for(i=1; i<argc; i++) {
    numerrors=0;
    if(process_file(argv[i])) {
      clock_t starttime, stoptime;

      symbol_init();
      starttime = clock();
      yyparse();
      smach_term();
      smach_run();
      stoptime = clock();
      symbol_end();
      fprintf(stderr, "Assembly of %s completed in %d ms, total errors %d\n",
	      argv[i], (int)((stoptime-starttime)/1000), numerrors);
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
  return 0;
}

