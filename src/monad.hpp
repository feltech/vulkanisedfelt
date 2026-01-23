#pragma once
#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <ranges>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <boost/hana/core/tag_of.hpp>
#include <boost/hana/fwd/ap.hpp>
#include <boost/hana/fwd/append.hpp>
#include <boost/hana/fwd/chain.hpp>
#include <boost/hana/fwd/fold_left.hpp>
#include <boost/hana/fwd/lift.hpp>

#include <libfork/algorithm/lift.hpp>
#include <libfork/core.hpp>
#include <libfork/core/control_flow.hpp>
#include <libfork/core/eventually.hpp>
#include <libfork/core/just.hpp>
#include <libfork/core/scheduler.hpp>
#include <libfork/core/task.hpp>
#include <libfork/schedule/lazy_pool.hpp>

#include <boost/hana.hpp>
#include <boost/hana/ext/std/integer_sequence.hpp>
#include <boost/hana/ext/std/tuple.hpp>
#include <boost/hana/fwd/core/to.hpp>
#include <boost/hana/fwd/transform.hpp>

#include <range/v3/view/filter.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/zip.hpp>

#include <immer/detail/rbts/bits.hpp>
#include <immer/vector.hpp>

#include "./monad/detail.hpp"
#include "./monad/io/IO.hpp"
#include "./monad/io/filter.hpp"
#include "./monad/io/fwd.hpp"
#include "./monad/io/hana.hpp"
#include "./monad/io/sequence.hpp"
#include "./monad/io/traverse.hpp"
#include "./monad/stateio/fwd.hpp"
#include "./monad/stateio/store.hpp"
#include "./monad/stateio/hana.hpp"
#include "./monad/stateio/StateIO.hpp"
#include "./monad/stateio/get_state.hpp"
#include "hof.hpp"