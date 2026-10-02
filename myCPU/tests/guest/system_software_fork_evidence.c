#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

static volatile uint64 cells[1024] __attribute__((aligned(4096)));

static void fail(const char *message)
{
  printf("EVIDENCE FAIL %s\n", message);
  exit(1);
}

int main(void)
{
  int parent = getpid();
  cells[0] = 0x11223344;
  cells[512] = 0x55667788;
  int child = fork();
  if(child < 0)
    fail("fork");
  if(child == 0){
    if(cells[0] != 0x11223344 || cells[512] != 0x55667788)
      fail("inheritance");
    printf("EVIDENCE BEFORE %d %d %p\n", parent, getpid(), (void*)cells);
    cells[0] = 0xaabbccdd;
    cells[512] = 0xeeff0011;
    printf("EVIDENCE AFTER %d %d %p\n", parent, getpid(), (void*)cells);
    exit(37);
  }
  int status = -1;
  if(wait(&status) != child || status != 37)
    fail("wait-status");
  if(cells[0] != 0x11223344 || cells[512] != 0x55667788)
    fail("parent-isolation");
  if(wait(0) != -1)
    fail("extra-child");
  printf("EVIDENCE REAP %d %d %p\n", parent, child, (void*)cells);
  exit(0);
}
