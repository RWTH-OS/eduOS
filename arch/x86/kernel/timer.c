#include <eduos/string.h>
#include <asm/io.h>
#include <asm/timer.h>

static unsigned long long CPUFREQ = 2500000000; //CPU Frequency in Hz

void sleep(unsigned int seconds){
  //uint64_t now = rdtsc();
  uint64_t stop = rdtsc() + seconds * CPUFREQ;
  while (rdtsc() <= stop) {
    //kprintf("Sleeping\n");
  }
}
