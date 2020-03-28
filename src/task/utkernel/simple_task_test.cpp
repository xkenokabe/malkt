﻿// Copyright(c) 2020 Ken Okabe
// This software is released under the MIT License, see LICENSE.
#include "gtest/gtest.h"

extern "C" {
#include "../../util/system_call_logger.h"
#include "simple_task.h"
#include "utkernel_tsk_spy.h"
}

namespace {
bool was_ran;

void DummyAction(void) { was_ran = true; }
}

class TaskTest : public ::testing::Test {
 protected:
  Task t;

  virtual void SetUp() {
    was_ran = false;
    utkernelTskSpy->Reset();
    systemCallLogger->Reset();
  }

  virtual void TearDown() {
    if (t != NULL) task->Delete(&t);
  }

  void NewSimpleTask() {
    t = simpleTask->New(DummyAction, 4, kMaxTaskStackSize);
    systemCallLogger->Reset();
  }
};

TEST_F(TaskTest, New) {
  t = simpleTask->New(DummyAction, 4, kMaxTaskStackSize);

  ASSERT_TRUE(t != NULL);
  EXPECT_EQ(TA_HLNG | TA_RNG0, utkernelTskSpy->Attribute());
  EXPECT_EQ(9, utkernelTskSpy->Priority());
  EXPECT_EQ(kMaxTaskStackSize, utkernelTskSpy->StackSize());
  EXPECT_FALSE(was_ran);
  EXPECT_STREQ(
      "+ tk_cre_tsk\n"
      "- tk_cre_tsk (0)\n",
      systemCallLogger->Get());
}

TEST_F(TaskTest, NewWithBoundaryPriority) {
  t = simpleTask->New(DummyAction, kHighestTaskPriority, kMaxTaskStackSize);
  EXPECT_EQ(5, utkernelTskSpy->Priority());
  task->Delete(&t);

  t = simpleTask->New(DummyAction, kLowestTaskPriority, kMaxTaskStackSize);
  EXPECT_EQ(12, utkernelTskSpy->Priority());
}

TEST_F(TaskTest, NewWithOutOfRangePriority) {
  t = simpleTask->New(DummyAction, kHighestTaskPriority + 1, kMaxTaskStackSize);
  EXPECT_EQ(5, utkernelTskSpy->Priority());
  task->Delete(&t);

  t = simpleTask->New(DummyAction, kLowestTaskPriority - 1, kMaxTaskStackSize);
  EXPECT_EQ(12, utkernelTskSpy->Priority());
}

TEST_F(TaskTest, NewWithOutOfRangeStackSize) {
  Task t = simpleTask->New(DummyAction, 4, kMaxTaskStackSize + 1);

  EXPECT_EQ(kMaxTaskStackSize, utkernelTskSpy->StackSize());
}

TEST_F(TaskTest, DeleteSelfTask) {
  NewSimpleTask();

  task->Delete(&t);

  EXPECT_EQ(NULL, t);
  EXPECT_STREQ(
      "+ tk_get_tid\n"
      "- tk_get_tid (0)\n"
      "+ tk_exd_tsk\n"
      "- tk_exd_tsk\n",
      systemCallLogger->Get());
}

TEST_F(TaskTest, DeleteOtherTask) {
  NewSimpleTask();
  utkernelTskSpy->SetReturnCode(0, 1);

  task->Delete(&t);

  EXPECT_EQ(NULL, t);
  EXPECT_STREQ(
      "+ tk_get_tid\n"
      "- tk_get_tid (1)\n"
      "+ tk_ter_tsk (0)\n"
      "- tk_ter_tsk (0)\n"
      "+ tk_del_tsk (0)\n"
      "- tk_del_tsk (0)\n",
      systemCallLogger->Get());
}

TEST_F(TaskTest, Run) {
  NewSimpleTask();

  task->Run(t);

  EXPECT_TRUE(was_ran);
  EXPECT_STREQ(
      "+ tk_sta_tsk (0)\n"
      "++ tk_ext_tsk\n"  // tk_ext_tsk() is called
      "-- tk_ext_tsk\n"  //  because the task action is not infinite loop.
      "- tk_sta_tsk (0)\n",
      systemCallLogger->Get());
}

TEST_F(TaskTest, SuspendSelfTask) {
  NewSimpleTask();

  task->Suspend(t);

  EXPECT_EQ(TMO_FEVR, utkernelTskSpy->Timeout());
  EXPECT_STREQ(
      "+ tk_get_tid\n"
      "- tk_get_tid (0)\n"
      "+ tk_slp_tsk\n"
      "- tk_slp_tsk (0)\n",  // Return because tk_slp_tsk() is test double.
      systemCallLogger->Get());
}

TEST_F(TaskTest, SuspendOtherTask) {
  NewSimpleTask();
  utkernelTskSpy->SetReturnCode(0, 1);

  task->Suspend(t);

  EXPECT_STREQ(
      "+ tk_get_tid\n"
      "- tk_get_tid (1)\n"
      "+ tk_sus_tsk (0)\n"
      "- tk_sus_tsk (0)\n"
      "+ tk_rel_wai (0)\n"
      "- tk_rel_wai (0)\n",
      systemCallLogger->Get());
}

TEST_F(TaskTest, SuspendMultipleTimes) {
  NewSimpleTask();

  task->Suspend(t);
  task->Suspend(t);
  task->Suspend(t);

  EXPECT_STREQ(
      "+ tk_get_tid\n"
      "- tk_get_tid (0)\n"
      "+ tk_slp_tsk\n"
      "- tk_slp_tsk (0)\n",
      systemCallLogger->Get());
}

TEST_F(TaskTest, ResumeTaskSuspendedByMyself) {
  NewSimpleTask();
  task->Suspend(t);
  systemCallLogger->Reset();

  task->Resume(t);

  EXPECT_STREQ(
      "+ tk_wup_tsk (0)\n"
      "- tk_wup_tsk (0)\n",
      systemCallLogger->Get());
}

TEST_F(TaskTest, ResumeTaskSuspendedByOther) {
  NewSimpleTask();
  utkernelTskSpy->SetReturnCode(0, 1);
  task->Suspend(t);
  systemCallLogger->Reset();

  task->Resume(t);

  EXPECT_STREQ(
      "+ tk_rsm_tsk (0)\n"
      "- tk_rsm_tsk (0)\n",
      systemCallLogger->Get());
}

TEST_F(TaskTest, ResumeTaskNotSuspended) {
  NewSimpleTask();

  task->Resume(t);

  EXPECT_STREQ("", systemCallLogger->Get());
}

TEST_F(TaskTest, ResumeMultipleTimes) {
  NewSimpleTask();
  task->Suspend(t);
  systemCallLogger->Reset();

  task->Resume(t);
  task->Resume(t);
  task->Resume(t);

  EXPECT_STREQ(
      "+ tk_wup_tsk (0)\n"
      "- tk_wup_tsk (0)\n",
      systemCallLogger->Get());
}