// A empty implementation of error to avoid including printf
extern "C" void __wrap_error(const char* format, ...) {
  while(1) {}
}
