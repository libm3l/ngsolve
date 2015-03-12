#ifndef FILE_TASKHANDLER
#define FILE_TASKHANDLER

/*********************************************************************/
/* File:   taskhandler.hpp                                           */
/* Author: M. Hochsterger, J. Schoeberl                              */
/* Date:   10. Mar. 2015                                             */
/*********************************************************************/


#ifdef USE_NUMA
#include <numa.h>
#include <sched.h>
#endif

#include <mutex>

namespace ngstd
{
  
  class TaskHandler
  {
    
    function<void(int)> func;

    atomic<int> jobnr;

    atomic<int> start_cnt[8]; // max nodes
    atomic<int> complete_cnt;
    atomic<int> done;
    atomic<int> participate;

    int num_nodes;
    
    mutex mtx;
    
  public:
    
    TaskHandler()
    {
#ifdef USE_NUMA
      numa_available();
      num_nodes = numa_max_node() + 1;
#else
      num_nodes = 1;
#endif

      participate = 0;
      jobnr = 0;
      done = 0;
    }

    void CreateTask (function<void(int)> afunc)
    {
      func = afunc;
      

      while (participate > 0);
      
      int oldval = 0;
      while (!participate.compare_exchange_weak (oldval, -1))
	oldval = 0;

      // tasks = _tasks;
      for (int j = 0; j < num_nodes; j++)
	start_cnt[j] = 0;
      complete_cnt = 0;
      jobnr++;
      
      participate = 0;



      int thd = omp_get_thread_num();
      int thds = omp_get_num_threads();
      
      int tasks_per_node = thds / num_nodes;
      int mynode = num_nodes * thd/thds;
      
#ifdef USE_NUMA
      numa_run_on_node (mynode);
#endif
      
      int mytask = start_cnt[mynode]++;
      if (mytask < tasks_per_node)
	{
	  func(mytask + mynode*tasks_per_node);
	  complete_cnt++;
	}
      
      while (complete_cnt < thds)
	;
    }
    
    void Done()
    {
      done = true;
    }

    void Loop()
    {
      int thd = omp_get_thread_num();
      int thds = omp_get_num_threads();
      
      int tasks_per_node = thds / num_nodes;
      int mynode = num_nodes * thd/thds;
      
#ifdef USE_NUMA
      numa_run_on_node (mynode);
#endif
      
      int jobdone = 0;
      
      while (!done)
	{
	  if (jobnr == jobdone)
	    {
	      sched_yield();
	      continue;
	    }
	  
	  bool dojob = false;
	  
	  while (participate == -1);
	  
	  int oldpart = 0;
	  while (! participate.compare_exchange_weak (oldpart, oldpart+1))
	    if (oldpart == -1) oldpart = 0;
	  

	  while (1)
	    {
	      int mytask = start_cnt[mynode]++;
	      if (mytask >= tasks_per_node) break;
	      
	      func(mytask + mynode*tasks_per_node);
	      
	      complete_cnt++;
	    }
	      
	  jobdone = jobnr;
	  participate--;
	}
    }
  };
  
}



#endif