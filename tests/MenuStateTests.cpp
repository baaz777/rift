// disabled menu entries must be skipped in both directions without losing wrap-around.

#include <gtest/gtest.h>

#include "../src/MenuLogic.hpp"

namespace
{

MenuLogic::ItemList MakeAllEnabled(int count, int initialSelection = 0)
{
    MenuLogic::ItemList list;
    list.enabled.assign(count, true);
    list.selected = initialSelection;
    return list;
}

}  // namespace

TEST(MenuLogic, NavigateDownAdvancesByOne)
{
    auto list = MakeAllEnabled(4);
    MenuLogic::NavigateDown(list);
    EXPECT_EQ(list.selected, 1);
}

TEST(MenuLogic, NavigateUpRetreatsByOne)
{
    auto list = MakeAllEnabled(4, 2);
    MenuLogic::NavigateUp(list);
    EXPECT_EQ(list.selected, 1);
}

TEST(MenuLogic, NavigateDownWrapsFromLastToFirst)
{
    auto list = MakeAllEnabled(4, 3);
    MenuLogic::NavigateDown(list);
    EXPECT_EQ(list.selected, 0);
}

TEST(MenuLogic, NavigateUpWrapsFromFirstToLast)
{
    auto list = MakeAllEnabled(4, 0);
    MenuLogic::NavigateUp(list);
    EXPECT_EQ(list.selected, 3);
}

TEST(MenuLogic, NavigateDownSkipsDisabled)
{
    MenuLogic::ItemList list;
    list.enabled = {true, false, false, true};
    list.selected = 0;

    MenuLogic::NavigateDown(list);
    EXPECT_EQ(list.selected, 3);  // skips both disabled items
}

TEST(MenuLogic, NavigateUpSkipsDisabled)
{
    MenuLogic::ItemList list;
    list.enabled = {true, false, false, true};
    list.selected = 3;

    MenuLogic::NavigateUp(list);
    EXPECT_EQ(list.selected, 0);
}

TEST(MenuLogic, NavigateDownWrapsAcrossDisabled)
{
    MenuLogic::ItemList list;
    list.enabled = {true, false, false, true};
    list.selected = 3;

    MenuLogic::NavigateDown(list);
    EXPECT_EQ(list.selected, 0);  // wraps past disabled items
}

TEST(MenuLogic, NavigateUpWrapsAcrossDisabled)
{
    MenuLogic::ItemList list;
    list.enabled = {true, false, false, true};
    list.selected = 0;

    MenuLogic::NavigateUp(list);
    EXPECT_EQ(list.selected, 3);
}

TEST(MenuLogic, NavigateNoOpWhenAllDisabledExceptCurrent)
{
    MenuLogic::ItemList list;
    list.enabled = {false, true, false, false};
    list.selected = 1;

    MenuLogic::NavigateDown(list);
    EXPECT_EQ(list.selected, 1);
    MenuLogic::NavigateUp(list);
    EXPECT_EQ(list.selected, 1);
}

TEST(MenuLogic, FirstEnabledIndexFindsFirstEnabled)
{
    MenuLogic::ItemList list;
    list.enabled = {false, false, true, true};
    EXPECT_EQ(MenuLogic::FirstEnabledIndex(list), 2);
}

TEST(MenuLogic, FirstEnabledIndexReturnsZeroWhenAllDisabled)
{
    MenuLogic::ItemList list;
    list.enabled = {false, false, false};
    EXPECT_EQ(MenuLogic::FirstEnabledIndex(list), 0);
}

TEST(MenuPrompt, DefaultsToCancel)
{
    MenuLogic::ConfirmPrompt p;
    EXPECT_EQ(p.selected, MenuLogic::ConfirmChoice::Cancel);
}

TEST(MenuPrompt, RightTogglesToConfirm)
{
    MenuLogic::ConfirmPrompt p;
    MenuLogic::ConfirmRight(p);
    EXPECT_EQ(p.selected, MenuLogic::ConfirmChoice::Confirm);
}

TEST(MenuPrompt, RightFromConfirmStaysOnConfirm)
{
    MenuLogic::ConfirmPrompt p;
    p.selected = MenuLogic::ConfirmChoice::Confirm;
    MenuLogic::ConfirmRight(p);
    EXPECT_EQ(p.selected, MenuLogic::ConfirmChoice::Confirm);
}

TEST(MenuPrompt, LeftFromConfirmReturnsToCancel)
{
    MenuLogic::ConfirmPrompt p;
    p.selected = MenuLogic::ConfirmChoice::Confirm;
    MenuLogic::ConfirmLeft(p);
    EXPECT_EQ(p.selected, MenuLogic::ConfirmChoice::Cancel);
}

TEST(MenuPrompt, LeftFromCancelStaysOnCancel)
{
    MenuLogic::ConfirmPrompt p;
    MenuLogic::ConfirmLeft(p);
    EXPECT_EQ(p.selected, MenuLogic::ConfirmChoice::Cancel);
}
