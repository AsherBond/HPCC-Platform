/*##############################################################################

    Copyright (C) 2021 HPCC Systems®.

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
############################################################################## */

#ifdef _USE_CPPUNIT
#include "loggingmanager.hpp"
#include "modularlogagent.ipp"
#include "unittests.hpp"
#include "logconfigptree.hpp"
#include <vector>
#include <functional>

using namespace ModularLogAgent;

class LoggingMockAgentTests : public CppUnit::TestFixture
{
    CPPUNIT_TEST_SUITE( LoggingMockAgentTests );
        CPPUNIT_TEST(testHasService_None);
        CPPUNIT_TEST(testHasService_All);
        CPPUNIT_TEST(testGetTransactionSeed_True);
        CPPUNIT_TEST(testGetTransactionSeed_False);
        CPPUNIT_TEST(testGetTransactionSeed_Bad);
        CPPUNIT_TEST(testUpdateLog_Good);
        CPPUNIT_TEST(testUpdateLog_Bad);
        CPPUNIT_TEST(testGetTransactionId_Good);
        CPPUNIT_TEST(testGetTransactionId_Bad);
    CPPUNIT_TEST_SUITE_END();
public:
#define checkHasService(agent, type, expected) \
    CPPUNIT_ASSERT_EQUAL(expected, agent->hasService(type))

    void testHasService_None()
    {
        const char* agentConfig = R"!!!(
            <LogAgent name="mock" module="mock"/>
        )!!!";
        Owned<IEspLogAgent> agent(createAgent(agentConfig));

        checkHasService(agent, LGSTGetTransactionSeed, false);
        checkHasService(agent, LGSTUpdateLOG, false);
        checkHasService(agent, LGSTGetTransactionID, false);
    }
    void testHasService_All()
    {
        const char* agentConfig = R"!!!(
            <LogAgent name="mock" module="mock">
              <GetTransactionSeed seed-id="12345" status-code="2" status-message="status 2"/>
              <UpdateLog response="update response" status-code="1" status-message="status 1"/>
              <GetTransactionId id="12345T67890"/>
            </LogAgent>
        )!!!";
        Owned<IEspLogAgent> agent(createAgent(agentConfig));

        checkHasService(agent, LGSTGetTransactionSeed, true);
        checkHasService(agent, LGSTUpdateLOG, true);
        checkHasService(agent, LGSTGetTransactionID, true);
    }

    void testGetTransactionSeed_True()
    {
        const char* agentConfig = R"!!!(
            <LogAgent name="mock" module="mock">
              <GetTransactionSeed seed-id="67890" status-code="0" status-message="status 0"/>
            </LogAgent>
        )!!!";
        Owned<IEspLogAgent> agent(createAgent(agentConfig));
        
        checkGetTransactionSeed("getTransactionSeed-true", agent, true, "67890", 0, "status 0");
    }
    void testGetTransactionSeed_False()
    {
        const char* agentConfig = R"!!!(
            <LogAgent name="mock" module="mock">
              <GetTransactionSeed seed-id="12345" status-code="2" status-message="status 2"/>
            </LogAgent>
        )!!!";
        Owned<IEspLogAgent> agent(createAgent(agentConfig));
        
        checkGetTransactionSeed("getTransactionSeed-false", agent, false, "12345", 2, "status 2");
    }
    void testGetTransactionSeed_Bad()
    {
        const char* agentConfig = R"!!!(
            <LogAgent name="mock" module="mock">
            </LogAgent>
        )!!!";
        Owned<IEspLogAgent> agent(createAgent(agentConfig));
        
        checkGetTransactionSeed("getTransactionSeed-bad", agent, false, "", -1, "unsupported request (getTransactionSeed)");
    }

    void testUpdateLog_Good()
    {
        const char* agentConfig = R"!!!(
            <LogAgent name="mock" module="mock">
              <UpdateLog response="update response" status-code="1" status-message="status 1"/>
            </LogAgent>
        )!!!";
        Owned<IEspLogAgent> agent(createAgent(agentConfig));

        checkUpdateLog("updateLog-good", agent, "update response", 1, "status 1");
    }
    void testUpdateLog_Bad()
    {
        const char* agentConfig = R"!!!(
            <LogAgent name="mock" module="mock">
            </LogAgent>
        )!!!";
        Owned<IEspLogAgent> agent(createAgent(agentConfig));

        checkUpdateLog("updateLog-bad", agent, "", -1, "unsupported request (updateLog)");
    }

    void testGetTransactionId_Good()
    {
        const char* agentConfig = R"!!!(
            <LogAgent name="mock" module="mock">
              <GetTransactionId id="12345T67890"/>
            </LogAgent>
        )!!!";
        Owned<IEspLogAgent> agent(createAgent(agentConfig));

        checkGetTransactionId("getTransactionId-good", agent, "12345T67890");
    }
    void testGetTransactionId_Bad()
    {
        const char* agentConfig = R"!!!(
            <LogAgent name="mock" module="mock">
            </LogAgent>
        )!!!";
        Owned<IEspLogAgent> agent(createAgent(agentConfig));

        checkGetTransactionId("getTransactionId-bad", agent, "unsupported request (getTransactionID)");
    }

    LoggingMockAgentTests() {}

    IEspLogAgent* createAgent(const char* agentConfig)
    {
        Owned<IPTree> config(createPTreeFromXMLString(agentConfig));
        Owned<CModuleFactory> factory(new CModuleFactory());
        Owned<CEspLogAgent> agent(new CEspLogAgent(*factory.getClear()));
        agent->init("mock", "LogAgent", config, "unittests");
        agent->initVariants(config);
        return agent.getClear();
    }
    void checkGetTransactionSeed(const char* test, IEspLogAgent* agent, bool expectResult, const char* expectSeedId, int expectStatusCode, const char* expectStatusMessage)
    {
        Owned<IEspGetTransactionSeedRequest> req(createGetTransactionSeedRequest());
        Owned<IEspGetTransactionSeedResponse> resp(createGetTransactionSeedResponse());

        CPPUNIT_ASSERT_EQUAL_MESSAGE(test, expectResult, agent->getTransactionSeed(*req, *resp));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(test, std::string(expectSeedId), std::string(resp->getSeedId()));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(test, expectStatusCode, resp->getStatusCode());
        CPPUNIT_ASSERT_EQUAL_MESSAGE(test, std::string(expectStatusMessage), std::string(resp->getStatusMessage()));
    }
    void checkUpdateLog(const char* test, IEspLogAgent* agent, const char* expectResponse, int expectStatusCode, const char* expectStatusMessage)
    {
        Owned<IEspUpdateLogRequestWrap> req(new CUpdateLogRequestWrap(nullptr, nullptr, nullptr));
        Owned<IEspUpdateLogResponse>    resp(createUpdateLogResponse());

        agent->updateLog(*req, *resp);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(test, std::string(expectResponse), std::string(resp->getResponse()));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(test, expectStatusCode, resp->getStatusCode());
        CPPUNIT_ASSERT_EQUAL_MESSAGE(test, std::string(expectStatusMessage), std::string(resp->getStatusMessage()));
    }
    void checkGetTransactionId(const char* test, IEspLogAgent* agent, const char* expect)
    {
        StringBuffer actual;

        agent->getTransactionID(nullptr, actual);
        CPPUNIT_ASSERT_EQUAL_MESSAGE(test, std::string(expect), std::string(actual.str()));
    }
};

CPPUNIT_TEST_SUITE_REGISTRATION( LoggingMockAgentTests );
CPPUNIT_TEST_SUITE_NAMED_REGISTRATION( LoggingMockAgentTests, "loggingmockagenttests" );

class LoggingIdFilterTests : public CppUnit::TestFixture
{
    constexpr static const char* managerConfig = R"!!!(
        <LoggingManager>
          <LogAgent name="alpha" type="LogAgent" plugin="modularlogagent" module="mock">
            <GetTransactionSeed seed-id="alpha" status-code="0" status-message="ok"/>
            <UpdateLog response="alpha response" status-code="0" status-message="ok"/>
            <GetTransactionId id="alpha1"/>
          </LogAgent>
          <LogAgent name="beta" type="LogAgent" plugin="modularlogagent" module="mock">
            <Variant group="foo"/>
            <GetTransactionSeed seed-id="beta" status-code="0" status-message="ok"/>
            <UpdateLog response="beta response" status-code="0" status-message="ok"/>
            <GetTransactionId id="beta1"/>
          </LogAgent>
          <LogAgent name="gamma" type="LogAgent" plugin="modularlogagent" module="mock">
            <Variant group="bar"/>
            <GetTransactionSeed seed-id="gamma" status-code="0" status-message="ok"/>
            <UpdateLog response="gamma response" status-code="0" status-message="ok"/>
            <GetTransactionId id="gamma1"/>
          </LogAgent>
          <LogAgent name="delta" type="LogAgent" plugin="modularlogagent" module="mock">
            <Variant group="foo"/>
            <Variant group="bar"/>
            <GetTransactionSeed seed-id="delta" status-code="0" status-message="ok"/>
            <UpdateLog response="delta response" status-code="0" status-message="ok"/>
            <GetTransactionId id="delta1"/>
          </LogAgent>
        </LoggingManager>
    )!!!";

    CLoggingManagerLoader m_loader;

    CPPUNIT_TEST_SUITE( LoggingIdFilterTests );
        CPPUNIT_TEST(testGroupFilter);
        CPPUNIT_TEST(testHasService);
        CPPUNIT_TEST(testGetTransactionSeed);
        CPPUNIT_TEST(testGetTransactionId);
    CPPUNIT_TEST_SUITE_END();

public:
    void testGroupFilter()
    {
        Owned<ILoggingManager> manager(createManager());
        checkGroupFilterWithHasFilteredService("groupfilter-seed", manager, LGSTGetTransactionSeed, "", 1, 4);
        checkGroupFilterWithHasFilteredService("groupfilter-seed", manager, LGSTGetTransactionSeed, "foo", 2, 3);
        checkGroupFilterWithHasFilteredService("groupfilter-seed", manager, LGSTGetTransactionSeed, "bar", 2, 3);
        checkGroupFilterWithHasFilteredService("groupfilter-seed", manager, LGSTGetTransactionSeed, "baz", 0, 5);
    }

    void testHasService()
    {
        Owned<ILoggingManager> manager(createManager());
        checkHasFilteredServiceForGroup("hasService-seed", manager, LGSTGetTransactionSeed, "", true);
        checkHasFilteredServiceForGroup("hasService-seed", manager, LGSTGetTransactionSeed, "foo", true);
        checkHasFilteredServiceForGroup("hasService-seed", manager, LGSTGetTransactionSeed, "bar", true);
        checkHasFilteredServiceForGroup("hasService-seed", manager, LGSTGetTransactionSeed, "baz", false);
    }

    void testGetTransactionSeed()
    {
        checkGetFilteredTransactionSeedForGroup("getTransactionSeed", "bar", true, "gamma", "Transaction Seed");
        checkGetFilteredTransactionSeedForGroup("getTransactionSeed", "baz", false, "Seed", "Failed");
    }

    void testGetTransactionId()
    {
        checkGetFilteredTransactionIdForGroup("getTransactionId", "bar", true, "gamma1", "");
        checkGetFilteredTransactionIdForGroup("getTransactionId", "baz", false, "", "");
    }

    LoggingIdFilterTests()
        : m_loader(nullptr, nullptr, nullptr, nullptr)
    {
    }

    ILoggingManager* createManager()
    {
        Owned<IPTree> config(createPTreeFromXMLString(managerConfig));
        Owned<ILoggingManager> manager(m_loader.create(*config));

        CPPUNIT_ASSERT(manager.get());
        CPPUNIT_ASSERT(manager->init(config, "unittest"));
        return manager.getClear();
    }

    void checkGroupFilterWithHasFilteredService(const char* test, ILoggingManager* manager, LOGServiceType service, const char* group, unsigned expectedPass, unsigned expectedFail)
    {
        using Variants = std::vector<Linked<const IEspLogAgentVariant> >;
        Variants pass;
        Variants fail;
        VariantGroupFilter groupFilter(group);
        EspLogAgentIdFilter wrapper = [&](const IEspLogAgentVariant& variant) {
            if (groupFilter(variant))
                pass.emplace_back(&variant);
            else
                fail.emplace_back(&variant);
            return false;
        };

        if (!group)
            group = "";

        manager->hasFilteredService(service, wrapper);
        CPPUNIT_ASSERT_MESSAGE(VStringBuffer("group '%s', pass (%u/%zu), fail (%u/%zu)", group, expectedPass, pass.size(), expectedFail, fail.size()), pass.size() == expectedPass && fail.size() == expectedFail);
        for (const Linked<const IEspLogAgentVariant>& v : pass)
            CPPUNIT_ASSERT_EQUAL_MESSAGE(VStringBuffer("group '%s', variant '%s/%s'", group, v->getName(), v->getGroup()), std::string(group), std::string(v->getGroup()));
    }
    void checkHasFilteredServiceForGroup(const char* test, ILoggingManager* manager, LOGServiceType service, const char* group, bool expectedResult)
    {
        if (!group)
            group = "";
        
        VariantGroupFilter groupFilter(group);

        CPPUNIT_ASSERT_EQUAL_MESSAGE(VStringBuffer("group '%s'", group), expectedResult, manager->hasFilteredService(service, groupFilter));
    }
    void checkGetFilteredTransactionSeedForGroup(const char* test, const char* group, bool expectedResult, const char* expectedSeed, const char* expectedStatus)
    {
        if (!group)
            group = "";
        
        Owned<ILoggingManager> manager(createManager());
        StringBuffer actualSeed, actualStatus;

        CPPUNIT_ASSERT_EQUAL_MESSAGE(VStringBuffer("group '%s'", group), expectedResult, manager->getFilteredTransactionSeed(actualSeed, actualStatus, VariantGroupFilter(group)));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(VStringBuffer("group '%s'", group), std::string(expectedSeed), std::string(actualSeed.str()));
        CPPUNIT_ASSERT_MESSAGE(VStringBuffer("group '%s'", group), hasPrefix(actualStatus, expectedStatus, true));
    }
    void checkGetFilteredTransactionIdForGroup(const char* test, const char* group, bool expectedResult, const char* expectedId, const char* expectedStatus)
    {
        if (!group)
            group = "";
        
        Owned<ILoggingManager> manager(createManager());
        StringBuffer actualId, actualStatus;

        CPPUNIT_ASSERT_EQUAL_MESSAGE(VStringBuffer("group '%s'", group), expectedResult, manager->getFilteredTransactionID(nullptr, actualId, actualStatus, VariantGroupFilter(group)));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(VStringBuffer("group '%s'", group), std::string(expectedId), std::string(actualId.str()));
        CPPUNIT_ASSERT_MESSAGE(VStringBuffer("group '%s'", group), hasPrefix(actualStatus, expectedStatus, true));
    }
};

CPPUNIT_TEST_SUITE_REGISTRATION( LoggingIdFilterTests );
CPPUNIT_TEST_SUITE_NAMED_REGISTRATION( LoggingIdFilterTests, "loggingidfiltertests" );


class LogConfigPTreeTests : public CppUnit::TestFixture
{
    CPPUNIT_TEST_SUITE(LogConfigPTreeTests);
        CPPUNIT_TEST(testIntegralSameSignedness);
        CPPUNIT_TEST(testIntegralMixedSignedness);
        CPPUNIT_TEST(testFloat);
        CPPUNIT_TEST(testMixedIntegralFloat);
    CPPUNIT_TEST_SUITE_END();

    void expectException(int expectedErrorCode, std::function<void()> func, const char* msg)
    {
        try {
            func();
            CPPUNIT_FAIL(VStringBuffer("Expected exception %d: %s", expectedErrorCode, msg).str());
        } catch (IException* e) {
            CPPUNIT_ASSERT_EQUAL_MESSAGE(msg, expectedErrorCode, e->errorCode());
            e->Release();
        }
    }

public:
    void testIntegralSameSignedness()
    {
        using namespace LogConfigPTree;
        
        // INT / INT: In bounds -> no throw
        applyDefault<int, int>(10, "UNITTEST");
        applyDefault<int, long long>(10LL, "UNITTEST");
        
        // Out of bounds long long -> int (both signed) expects ERROR
        expectException(999, []{ applyDefault<int, long long>(99999999999999LL, "UNITTEST"); }, "long long > int::max");
        expectException(999, []{ applyDefault<int, long long>(-99999999999999LL, "UNITTEST"); }, "long long < int::min");

        // UINT / UINT: In bounds -> no throw
        applyDefault<unsigned, unsigned>(10U, "UNITTEST");
        
        // Out of bounds unsigned long long -> unsigned expects ERROR
        expectException(999, []{ applyDefault<unsigned, unsigned long long>(99999999999999ULL, "UNITTEST"); }, "unsigned long long > unsigned::max");
    }

    void testIntegralMixedSignedness()
    {
        using namespace LogConfigPTree;

        // INT / UINT (Signed value, unsigned default): out of bounds MAX -> ERROR
        expectException(999, []{ applyDefault<int, unsigned>(0xFFFFFFFFU, "UNITTEST"); }, "unsigned > int::max");

        // UINT / INT (Unsigned value, signed default): out of bounds (<0 or >max) -> WARN
        expectException(998, []{ applyDefault<unsigned, int>(-1, "UNITTEST"); }, "signed < 0 for unsigned");
    }

    void testFloat()
    {
        using namespace LogConfigPTree;

        // FLOAT / FLOAT: In bounds -> no throw
        // Added to prevent regression that routes two floats to the integral block
        // which then fails the bounds check and throws an exception erroneously.
        applyDefault<float, double>(-1.0, "UNITTEST");

        // These legitimately exceed float bounds -> ERROR
        expectException(999, []{ applyDefault<float, double>(1e300, "UNITTEST"); }, "double > float::max");
        expectException(999, []{ applyDefault<float, double>(-1e300, "UNITTEST"); }, "double < lowest float");
    }

    void testMixedIntegralFloat()
    {
        using namespace LogConfigPTree;

        // FLOAT / INT -> WARN
        expectException(998, []{ applyDefault<float, int>(10, "UNITTEST"); }, "float vs int should WARN (998)");

        // INT / FLOAT -> WARN
        expectException(998, []{ applyDefault<int, float>(10.0f, "UNITTEST"); }, "int vs float should WARN (998)");
    }

};

CPPUNIT_TEST_SUITE_REGISTRATION( LogConfigPTreeTests );
CPPUNIT_TEST_SUITE_NAMED_REGISTRATION( LogConfigPTreeTests, "logconfigptreetests" );

#endif // _USE_CPPUNIT