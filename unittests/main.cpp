/*
  This file includes the magic necessary in order to get your unit tests
  that you create with UnitTest++ to automatically run. There should
  never be a reason to modify this file.
  For a reference on creating tests, see gsTutorial.cpp
 */

#include "gismo_unittest.h"
#include "TestReporterStdout.h"

// Tolerance for approximate comparisons
const real_t EPSILON = std::pow(10.0, - REAL_DIG * 0.75);

/// Selects tests by matching their names to the input prefix given by command line argument
// TODO implement more complex queries
class Selector
{
private:
    int            m_argc;
    char        ** m_argv;
    mutable bool   m_did_run;

public:
    Selector(int argc, char* argv[])
    : m_argc(argc), m_argv(argv), m_did_run(false)
    { }

    bool operator()(const UnitTest::Test * const testCase) const
    {
        bool toRun = false;
        for (int i=1; i<m_argc; ++i)
        {
            const size_t n = strlen(m_argv[i]);
            toRun |= !strncmp(testCase->m_details.suiteName, m_argv[i], n);// prefix match
            toRun |= !strncmp(testCase->m_details.testName , m_argv[i], n);// prefix match
            toRun |= gsFileManager::pathEqual(testCase->m_details.filename, m_argv[i]);// exact match up to path sep.
        }
        m_did_run |= toRun;
        return toRun;
    }

    bool didRunAnyTests() { return m_did_run; }
};


int main(int argc, char* argv[])
{
    gsCmdLine::printVersion();

    if (argc > 1)
    {
        UnitTest::TestReporterStdout reporter;
        UnitTest::TestRunner runner(reporter);
        Selector sel(argc,argv);
        int result = runner.RunTestsIf(UnitTest::Test::GetTestList(), NULL, sel, 0);
        if (!sel.didRunAnyTests())
        {
            gsInfo << "Did not find any matching test.\n";
            return 1;
        }
        return result;
    }
    else
    {
        return UnitTest::RunAllTests();
    }
}
