// Every HDF5 call spine-reader makes, closing a file included, happens with core::hdf5_mutex()
// held, so that the library is safe in applications whose HDF5 is not thread-safe.
//
// The test holds the lock on one thread and starts the public operations on others. An
// operation that takes the lock cannot finish while it is held; one that does not, finishes at
// once. A control operation that touches no HDF5 must finish, so that a harness that never lets
// anything finish cannot make this pass.

#include <doctest.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <spine/reader.hpp>

#include "core/source.hpp"

using namespace spine;

namespace {

std::string fixture(const std::string& name) {
  return std::string(SPINE_READER_TEST_DATA) + "/fixtures/" + name;
}

/// Starts every action on a thread of its own while another thread holds the HDF5 lock, waits
/// 150 ms, and reports which actions have finished. Then releases the lock and joins; the
/// actions must all finish by themselves.
std::vector<bool> finished_while_locked(const std::vector<std::function<void()>>& actions) {
  std::promise<void> held;
  std::promise<void> release;
  std::future<void> release_signal = release.get_future();
  std::thread holder([&] {
    std::lock_guard<std::recursive_mutex> lock(core::hdf5_mutex());
    held.set_value();
    release_signal.wait();
  });
  held.get_future().wait();

  std::vector<std::atomic<bool>> done(actions.size());
  for (auto& flag : done) flag = false;
  std::vector<std::thread> workers;
  for (std::size_t i = 0; i < actions.size(); ++i) {
    workers.emplace_back([&, i] {
      actions[i]();
      done[i] = true;
    });
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(150));
  std::vector<bool> finished;
  for (const auto& flag : done) finished.push_back(flag);
  release.set_value();
  for (std::thread& worker : workers) worker.join();
  holder.join();
  return finished;
}

}  // namespace

TEST_CASE("the public operations of a Reader that touch HDF5 wait for the HDF5 lock") {
  const std::string path = fixture("v1.4.0_v1.h5");
  Reader reader(path);
  auto doomed = std::make_unique<Reader>(path);  // destroyed by an action
  Reader target(path);                           // overwritten by an action
  Reader source(fixture("v1.4.0_v2.h5"));        // moved from by an action
  std::unique_ptr<Reader> constructed;           // built by an action (and so destroyed after)

  const std::vector<bool> finished = finished_while_locked({
      [&] { constructed = std::make_unique<Reader>(path); },  // opens the file
      [&] { (void)reader.event(0); },                         // reads objects
      [&] { (void)reader.find(1, 2, 3); },                    // builds the run index on first use
      [&] { doomed.reset(); },                                // closes the files
      [&] { target = std::move(source); },                    // closes the files target held
      [&] {                                                   // no HDF5 at all
        (void)reader.num_events();
        (void)reader.schema_report();
      },
  });
  CHECK_FALSE(finished[0]);  // construction
  CHECK_FALSE(finished[1]);  // event()
  CHECK_FALSE(finished[2]);  // find()
  CHECK_FALSE(finished[3]);  // destruction
  CHECK_FALSE(finished[4]);  // move assignment
  CHECK(finished[5]);        // the control

  // and once the lock was free, they all did what they were asked
  CHECK(doomed == nullptr);
  REQUIRE(constructed != nullptr);
  CHECK(constructed->num_events() == 5);
  CHECK(target.num_events() == 5);
  CHECK(target.schema_report().layout == "V2Products");
  CHECK(reader.event(0).run_info() != nullptr);
}
