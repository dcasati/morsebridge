#include "../MorseBridge/ReportSession.h"

#include <cassert>
#include <cstdio>
#include <vector>

using MorseBridge::SendResult;

int main() {
  MorseBridge::ReportSession session;
  uint32_t now = UINT32_MAX - 20;
  SendResult result = SendResult::Accepted;
  std::vector<uint8_t> attempts;
  unsigned aborts = 0;
  auto update = [&](bool dit, bool dah, bool selected = true) {
    session.update(dit, dah, now, selected, true, true, 1,
      [&](const MorseBle::KeyboardReport &report) {
        attempts.push_back(report.modifiers);
        return result;
      }, [&] { ++aborts; });
  };
  auto sample = [&](bool dit, bool dah, unsigned count) {
    for (unsigned i = 0; i < count; ++i, ++now) update(dit, dah);
  };
  session.reset(now);
  sample(false, false, 10);
  assert(session.armed() && attempts == std::vector<uint8_t>{0});

  // A failed press may have reached the host. A cancelled press still needs key-up.
  result = SendResult::Failed;
  sample(true, false, 6);
  assert(attempts.back() == 1 && !session.released(true));
  sample(false, false, 6);
  result = SendResult::Accepted;
  sample(false, false, 10);
  assert(attempts.back() == 0 && session.released(true) && aborts == 0);

  // Likewise, a failed release followed by a new hold must refresh the held state.
  sample(true, false, 6);
  assert(attempts.back() == 1);
  result = SendResult::Failed;
  sample(false, false, 6);
  assert(attempts.back() == 0);
  sample(true, false, 6);
  result = SendResult::Accepted;
  sample(true, false, 10);
  assert(attempts.back() == 1 && !session.released(true) && aborts == 0);
  sample(false, false, 6);

  // Cancelling a busy (not queued) press ends that wait, not a later wait.
  result = SendResult::Busy;
  sample(true, false, 6);
  sample(false, false, 6);
  const auto idleAttempts = attempts.size();
  sample(false, false, 300);
  assert(attempts.size() == idleAttempts && aborts == 0);
  sample(true, false, 6);
  assert(aborts == 0);
  sample(true, false, 249);
  assert(aborts == 0);
  sample(true, false, 1);
  assert(aborts == 1);

  // Readiness invalidation must not erase the old route's held-state obligation.
  session.reset(now);
  result = SendResult::Accepted;
  sample(false, false, 10);
  sample(true, false, 6);
  assert(!session.released(true));
  session.update(true, false, now, false, true, false, 2,
    [](const MorseBle::KeyboardReport &) { assert(false); return SendResult::Failed; },
    [&] { ++aborts; });
  assert(!session.released(true) && session.released(false) && aborts == 2);
  std::puts("Shared report session tests passed");
}
