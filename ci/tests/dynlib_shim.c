/* To be defined in a shared library */
int bench_main(int argc, char *argv[]);

int main(int argc, char *argv[])
{
  return bench_main(argc, argv);
}
