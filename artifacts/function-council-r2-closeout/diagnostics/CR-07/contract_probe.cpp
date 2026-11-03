// The loop shape of forgeshape_history_selftest.cpp:466-467: is a*b+c fused?
#include <cstddef>
double feed(std::size_t i) { return 0.5 + 0.01 * static_cast<double>(i); }
