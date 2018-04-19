/*
** Author(s):
**  - Cedric GESTES <gestes@aldebaran-robotics.com>
**
** Copyright (C) 2010 Aldebaran Robotics
*/

#include <gtest/gtest.h>
#include <map>
#include <qi/functors/functor.hpp>
#include <qi/functors/makefunctor.hpp>
#include <qi/functors/callfunctor.hpp>
#include <qi/messaging/perf/dataperftimer.hpp>
#include <cmath>


static const int gLoopCount   = 1000000;

static int gGlobalResult = 0;

void vfun0()                                                                                      { gGlobalResult = 0; }
void vfun1(const int &p0)                                                                         { gGlobalResult = p0; }
void vfun2(const int &p0,const int &p1)                                                           { gGlobalResult = p0 + p1; }
void vfun3(const int &p0,const int &p1,const int &p2)                                             { gGlobalResult = p0 + p1 + p2; }
void vfun4(const int &p0,const int &p1,const int &p2,const int &p3)                               { gGlobalResult = p0 + p1 + p2 + p3; }
void vfun5(const int &p0,const int &p1,const int &p2,const int &p3,const  int &p4)                { gGlobalResult = p0 + p1 + p2 + p3 + p4; }
void vfun6(const int &p0,const int &p1,const int &p2,const int &p3,const  int &p4,const  int &p5) { gGlobalResult = p0 + p1 + p2 + p3 + p4 + p5; }

int fun0()                                                                                      { return 0; }
int fun1(const int &p0)                                                                         { return p0; }
int fun2(const int &p0,const int &p1)                                                           { return p0 + p1; }
int fun3(const int &p0,const int &p1,const int &p2)                                             { return p0 + p1 + p2; }
int fun4(const int &p0,const int &p1,const int &p2,const int &p3)                               { return p0 + p1 + p2 + p3; }
int fun5(const int &p0,const int &p1,const int &p2,const int &p3,const  int &p4)                { return p0 + p1 + p2 + p3 + p4; }
int fun6(const int &p0,const int &p1,const int &p2,const int &p3,const  int &p4,const  int &p5) { return p0 + p1 + p2 + p3 + p4 + p5; }


struct Foo {
  void voidCall()                                          { return; }
  int intStringCall(const std::string &plouf)              { return plouf.size(); }

  int fun0()                                                                                      { return 0; }
  int fun1(const int &p0)                                                                         { return p0; }
  int fun2(const int &p0,const int &p1)                                                           { return p0 + p1; }
  int fun3(const int &p0,const int &p1,const int &p2)                                             { return p0 + p1 + p2; }
  int fun4(const int &p0,const int &p1,const int &p2,const int &p3)                               { return p0 + p1 + p2 + p3; }
  int fun5(const int &p0,const int &p1,const int &p2,const int &p3,const  int &p4)                { return p0 + p1 + p2 + p3 + p4; }
  int fun6(const int &p0,const int &p1,const int &p2,const int &p3,const  int &p4,const  int &p5) { return p0 + p1 + p2 + p3 + p4 + p5; }

  void vfun0()                                                                                      { gGlobalResult = 0; }
  void vfun1(const int &p0)                                                                         { gGlobalResult = p0; }
  void vfun2(const int &p0,const int &p1)                                                           { gGlobalResult = p0 + p1; }
  void vfun3(const int &p0,const int &p1,const int &p2)                                             { gGlobalResult = p0 + p1 + p2; }
  void vfun4(const int &p0,const int &p1,const int &p2,const int &p3)                               { gGlobalResult = p0 + p1 + p2 + p3; }
  void vfun5(const int &p0,const int &p1,const int &p2,const int &p3,const  int &p4)                { gGlobalResult = p0 + p1 + p2 + p3 + p4; }
  void vfun6(const int &p0,const int &p1,const int &p2,const int &p3,const  int &p4,const  int &p5) { gGlobalResult = p0 + p1 + p2 + p3 + p4 + p5; }
};


TEST(TestBind, MultiArgMember) {
  Foo          foo;
  qi::Functor *functor;

  functor = qi::makeFunctor(&foo, &Foo::fun0);
  EXPECT_EQ(0 , qi::callFunctor<int>(functor));
  functor = qi::makeFunctor(&foo, &Foo::fun1);
  EXPECT_EQ(1 , qi::callFunctor<int>(functor, 1));
  functor = qi::makeFunctor(&foo, &Foo::fun2);
  EXPECT_EQ(3 , qi::callFunctor<int>(functor, 1, 2));
  functor = qi::makeFunctor(&foo, &Foo::fun3);
  EXPECT_EQ(6 , qi::callFunctor<int>(functor, 1, 2, 3));
  functor = qi::makeFunctor(&foo, &Foo::fun4);
  EXPECT_EQ(10, qi::callFunctor<int>(functor, 1, 2, 3, 4));
  functor = qi::makeFunctor(&foo, &Foo::fun5);
  EXPECT_EQ(15, qi::callFunctor<int>(functor, 1, 2, 3, 4, 5));
  functor = qi::makeFunctor(&foo, &Foo::fun6);
  EXPECT_EQ(21, qi::callFunctor<int>(functor, 1, 2, 3, 4, 5, 6));
}

TEST(TestBind, MultiArgVoidMember) {
  Foo          foo;
  qi::Functor *functor;

  functor = qi::makeFunctor(&foo, &Foo::vfun0);
  qi::callVoidFunctor(functor);
  EXPECT_EQ(0, gGlobalResult);

  functor = qi::makeFunctor(&foo, &Foo::vfun1);
  qi::callVoidFunctor(functor, 1);
  EXPECT_EQ(1, gGlobalResult);

  functor = qi::makeFunctor(&foo, &Foo::vfun2);
  qi::callVoidFunctor(functor, 1, 2);
  EXPECT_EQ(3, gGlobalResult);

  functor = qi::makeFunctor(&foo, &Foo::vfun3);
  qi::callVoidFunctor(functor, 1, 2, 3);
  EXPECT_EQ(6, gGlobalResult);

  functor = qi::makeFunctor(&foo, &Foo::vfun4);
  qi::callVoidFunctor(functor, 1, 2, 3, 4);
  EXPECT_EQ(10, gGlobalResult);

  functor = qi::makeFunctor(&foo, &Foo::vfun5);
  qi::callVoidFunctor(functor, 1, 2, 3, 4, 5);
  EXPECT_EQ(15, gGlobalResult);

  functor = qi::makeFunctor(&foo, &Foo::vfun6);
  qi::callVoidFunctor(functor, 1, 2, 3, 4, 5, 6);
  EXPECT_EQ(21, gGlobalResult);
}


TEST(TestBind, MultiArgFun) {
  qi::Functor *functor;

  functor = qi::makeFunctor(&fun0);
  EXPECT_EQ(0 , qi::callFunctor<int>(functor));
  functor = qi::makeFunctor(&fun1);
  EXPECT_EQ(1 , qi::callFunctor<int>(functor, 1));
  functor = qi::makeFunctor(&fun2);
  EXPECT_EQ(3 , qi::callFunctor<int>(functor, 1, 2));
  functor = qi::makeFunctor(&fun3);
  EXPECT_EQ(6 , qi::callFunctor<int>(functor, 1, 2, 3));
  functor = qi::makeFunctor(&fun4);
  EXPECT_EQ(10, qi::callFunctor<int>(functor, 1, 2, 3, 4));
  functor = qi::makeFunctor(&fun5);
  EXPECT_EQ(15, qi::callFunctor<int>(functor, 1, 2, 3, 4, 5));
  functor = qi::makeFunctor(&fun6);
  EXPECT_EQ(21, qi::callFunctor<int>(functor, 1, 2, 3, 4, 5, 6));
}

TEST(TestBind, MultiArgVoidFun) {
  qi::Functor *functor;

  functor = qi::makeFunctor(&vfun0);
  qi::callVoidFunctor(functor);
  EXPECT_EQ(0, gGlobalResult);

  functor = qi::makeFunctor(&vfun1);
  qi::callVoidFunctor(functor, 1);
  EXPECT_EQ(1, gGlobalResult);

  functor = qi::makeFunctor(&vfun2);
  qi::callVoidFunctor(functor, 1, 2);
  EXPECT_EQ(3, gGlobalResult);

  functor = qi::makeFunctor(&vfun3);
  qi::callVoidFunctor(functor, 1, 2, 3);
  EXPECT_EQ(6, gGlobalResult);

  functor = qi::makeFunctor(&vfun4);
  qi::callVoidFunctor(functor, 1, 2, 3, 4);
  EXPECT_EQ(10, gGlobalResult);

  functor = qi::makeFunctor(&vfun5);
  qi::callVoidFunctor(functor, 1, 2, 3, 4, 5);
  EXPECT_EQ(15, gGlobalResult);

  functor = qi::makeFunctor(&vfun6);
  qi::callVoidFunctor(functor, 1, 2, 3, 4, 5, 6);
  EXPECT_EQ(21, gGlobalResult);
}

TEST(TestBind, VoidCallPerf) {
  Foo           chiche;
  Foo          *p = &chiche;
  qi::DataStream   res;
  qi::DataStream  cd;

  qi::perf::DataPerfTimer dp;
  qi::Functor    *functor = qi::makeFunctor(&chiche, &Foo::voidCall);
  std::cout << "qi::Functor call" << std::endl;
  dp.start(gLoopCount);
  for (int i = 0; i < gLoopCount; ++i)
  {
    functor->call(cd, res);
  }
  dp.stop();

  std::cout << "pointer call" << std::endl;
  dp.start(gLoopCount);
  for (int i = 0; i < gLoopCount; ++i)
  {
    p->voidCall();
  }
  dp.stop();
}

TEST(TestBind, IntStringCallPerf) {
  Foo           chiche;
  Foo          *p = &chiche;
  qi::DataStream res;

  qi::perf::DataPerfTimer dp;

  std::cout << "qi::Functor call (string with a growing size)" << std::endl;

  for (int i = 0; i < 12; ++i)
  {
    unsigned int    numBytes = (unsigned int)pow(2.0f,(int)i);
    std::string     request = std::string(numBytes, 'B');
    qi::DataStream  cd;
    qi::Functor    *functor = qi::makeFunctor(&chiche, &Foo::intStringCall);

    cd.writeString(request);
    dp.start(gLoopCount, numBytes);
    for (int j = 0; j < gLoopCount; ++j) {
      functor->call(cd, res);
    }
    dp.stop();
  }

  std::cout << "pointer call (string with a growing size)" << std::endl;
  for (int i = 0; i < 12; ++i)
  {
    unsigned int    numBytes = (unsigned int)pow(2.0f,(int)i);
    std::string     request = std::string(numBytes, 'B');

    dp.start(gLoopCount, numBytes);
    for (int j = 0; j < gLoopCount; ++j) {
      p->intStringCall(request);
    }
    dp.stop();
  }

}
