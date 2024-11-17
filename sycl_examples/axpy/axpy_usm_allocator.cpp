#include <cassert>
#include <iostream>
#include <sycl/sycl.hpp>

using namespace sycl;

// shortcut for USM host allocator class
template <typename T>
using host_allocator = sycl::usm_allocator<T, usm::alloc::host>;
// define shortcut for USM shared allocator class
template <typename T>
using shared_allocator = usm_allocator<T, usm::alloc::shared>;

template <typename T>
// what should be the second template argument of the return type?
std::vector<T, shared_allocator<T>>
axpy(
  queue& Q,
  T alpha,
  // what should be the second template argument of the input types?
  const std::vector<T, host_allocator<T>>& x,
  const std::vector<T, host_allocator<T>>& y)
{
  assert(x.size() == y.size());
  auto sz = x.size();

  auto ptrX = x.data();
  auto ptrY = y.data();

  //allocate output vector
  std::vector z(sz, T{ 0.0 }, shared_allocator<T>(Q));
  // why do we take the address of the output vector?
  // get address of z, because we rely on by-copy capture in the kernel lambda.
  // Why?
  //  1. by-copy capture gives a const object in the kernel, resulting in a
  //  compiler error.
  //  2. by-reference capture gives a runtime error.
  auto ptrZ = z.data();

  Q.submit([&](handler& cgh) {
     cgh.parallel_for(range { sz }, [=](id<1> tid) {
       // implement AXPY kernel
         auto i = tid[0];
         ptrZ[i] = ptrX[i] * alpha + ptrY[i];

     });
   }).wait();

  return z;
}

int main()
{
  constexpr auto sz = 1024;

  constexpr auto alpha = 1.0;

  queue Q;

  std::cout << "Running on: " << Q.get_device().get_info<info::device::name>()
            << std::endl;

  // create an allocator object on the queue for the operands.
  // Should it be host or shared?
  auto h_alloc = host_allocator<double>(Q);//Q?

  // create and fill vector x with 0, 1, 2, ..., sz-1
  std::vector x(sz,0.0, h_alloc);
  std::iota(x.begin(), x.end(), 0.0);
  // create and fill vector y with sz-1, sz-2, ..., 1, 0
  std::vector y(sz,0.0, h_alloc);
  std::iota(y.rbegin(), y.rend(), 0.0);

  auto z = axpy<double>(Q, alpha, x, y);

  std::cout << "Checking results..." << std::endl;
  auto message = "Nice job!";
  for (auto i = 0; i < sz; ++i) {
    if (std::abs(z[i] - (sz - 1)) >= 1.0e-13) {
      std::cout << "Uh-oh!" << std::endl;
      std::cout << z[i] << std::endl;
      message = "Not quite there yet :(";
      break;
    }
  }
  std::cout << message << std::endl;
  return 0;
}