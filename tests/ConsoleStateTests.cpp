// console state cycle: Closed -> Half -> Full -> Closed.

#include <gtest/gtest.h>

#include "../src/Console.hpp"

TEST(ConsoleStateTests, ClosedAdvancesToHalf)
{
    EXPECT_EQ(NextConsoleState(Console::State::Closed), Console::State::Half);
}

TEST(ConsoleStateTests, HalfAdvancesToFull)
{
    EXPECT_EQ(NextConsoleState(Console::State::Half), Console::State::Full);
}

TEST(ConsoleStateTests, FullAdvancesToClosed)
{
    EXPECT_EQ(NextConsoleState(Console::State::Full), Console::State::Closed);
}

TEST(ConsoleStateTests, ThreeStepCycleReturnsToStart)
{
    Console::State s = Console::State::Closed;
    s = NextConsoleState(s);
    s = NextConsoleState(s);
    s = NextConsoleState(s);
    EXPECT_EQ(s, Console::State::Closed);
}

TEST(ConsoleStateTests, FunctionIsConstexpr)
{
    constexpr auto a = NextConsoleState(Console::State::Closed);
    constexpr auto b = NextConsoleState(Console::State::Half);
    constexpr auto c = NextConsoleState(Console::State::Full);
    static_assert(a == Console::State::Half);
    static_assert(b == Console::State::Full);
    static_assert(c == Console::State::Closed);
}
