/*##############################################################################

    HPCC SYSTEMS software Copyright (C) 2022 HPCC Systems®.

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

#include "AzureLogAnalyticsCurlClient.hpp"

#include "platform.h"
#include <curl/curl.h>
#include <string>
#include <vector>

#include <cstdio>
#include <iostream>
#include <stdexcept>

static constexpr const char * defaultTSName = "TimeGenerated";
static constexpr const char * defaultIndexPattern = "ContainerLog";

static constexpr const char * defaultHPCCLogSeqCol         = "hpcc_log_sequence";
static constexpr const char * defaultHPCCLogTimeStampCol   = "hpcc_log_timestamp";
static constexpr const char * defaultHPCCLogProcIDCol      = "hpcc_log_procid";
static constexpr const char * defaultHPCCLogThreadIDCol    = "hpcc_log_threadid";
static constexpr const char * defaultHPCCLogMessageCol     = "hpcc_log_message";
static constexpr const char * defaultHPCCLogJobIDCol       = "hpcc_log_jobid";
static constexpr const char * defaultHPCCLogComponentCol   = "hpcc_log_component";
static constexpr const char * defaultHPCCLogTypeCol        = "hpcc_log_class";
static constexpr const char * defaultHPCCLogAudCol         = "hpcc_log_audience";
static constexpr const char * defaultHPCCLogTraceIDCol     = "hpcc_log_traceid";
static constexpr const char * defaultHPCCLogSpanIDCol      = "hpcc_log_spanid";
static constexpr const char * defaultHPCCLogComponentTSCol = "TimeGenerated";
static constexpr const char * defaultPodHPCCLogCol         = "PodName";

static constexpr const char * logMapIndexPatternAtt = "@storeName";
static constexpr const char * logMapSearchColAtt = "@searchColumn";
static constexpr const char * logMapTimeStampColAtt = "@timeStampColumn";
static constexpr const char * logMapKeyColAtt = "@keyColumn";
static constexpr const char * logMapDisableJoinsAtt = "@disableJoins";

static constexpr std::size_t  defaultMaxRecordsPerFetch = 100;

static size_t captureIncomingCURLReply(void* contents, size_t size, size_t nmemb, void* userp)
{
    size_t          incomingDataSize = size * nmemb;
    MemoryBuffer*   mem = static_cast<MemoryBuffer*>(userp);
    static constexpr size_t MAX_BUFFER_SIZE = 4194304; // 2^22

    if ((mem->length() + incomingDataSize) < MAX_BUFFER_SIZE)
    {
        mem->append(incomingDataSize, contents);
    }
    else
    {
        // Signals an error to libcurl
        incomingDataSize = 0;
        WARNLOG("%s::captureIncomingCURLReply exceeded buffer size %zu", COMPONENT_NAME, MAX_BUFFER_SIZE);
    }

    return incomingDataSize;
}

static void requestLogAnalyticsAccessToken(StringBuffer & token, const char * clientID, const char * clientSecret, const char * tenantID)
{
    if (isEmptyString(clientID))
        throw makeStringExceptionV(-1, "%s Access token request: Azure Active Directory Application clientID is required!", COMPONENT_NAME);

    if (isEmptyString(tenantID))
        throw makeStringExceptionV(-1, "%s Access token request: Azure tenantID is required!", COMPONENT_NAME);

    if (isEmptyString(clientSecret))
        throw makeStringExceptionV(-1, "%s Access token request: Azure Active Directory Application Secret is required!", COMPONENT_NAME);

    OwnedPtrCustomFree<CURL, curl_easy_cleanup> curlHandle = curl_easy_init();
    if (curlHandle)
    {
        CURLcode                curlResponseCode;
        static constexpr size_t initialBufferSize = 32768; // 2^15
        MemoryBuffer            captureBuffer(initialBufferSize);

        VStringBuffer tokenRequestURL("https://login.microsoftonline.com/%s/oauth2/token", tenantID);
        VStringBuffer tokenRequestFields("grant_type=client_credentials&resource=https://api.loganalytics.io&client_secret=%s&client_id=%s", clientSecret, clientID);

            /*"curl -X POST -d 'grant_type=client_credentials&client_id=<ADApplicationID>&client_secret=<thesecret>&resource=https://api.loganalytics.io'
               https://login.microsoftonline.com/<tenantID>/oauth2/token
            "*/

        if (curl_easy_setopt(curlHandle, CURLOPT_URL, tokenRequestURL.str()) != CURLE_OK)
            throw makeStringExceptionV(-1, "%s: Access token request: Could not set 'CURLOPT_URL' (%s)!", COMPONENT_NAME,  tokenRequestURL.str());

        if (curl_easy_setopt(curlHandle, CURLOPT_POST, 1) != CURLE_OK)
            throw makeStringExceptionV(-1, "%s Access token request: Could not set 'CURLOPT_POST' option!", COMPONENT_NAME);

        if (curl_easy_setopt(curlHandle, CURLOPT_POSTFIELDS, tokenRequestFields.str()) != CURLE_OK)
            throw makeStringExceptionV(-1, "%s Access token request: Could not set 'CURLOPT_POSTFIELDS' option!", COMPONENT_NAME);

        if (curl_easy_setopt(curlHandle, CURLOPT_NOPROGRESS, 1) != CURLE_OK)
            throw makeStringExceptionV(-1, "%s Access token request: Could not disable 'CURLOPT_NOPROGRESS' option!", COMPONENT_NAME);

        if (curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, captureIncomingCURLReply) != CURLE_OK)
            throw makeStringExceptionV(-1, "%s Access token request: Could not set 'CURLOPT_WRITEFUNCTION' option!", COMPONENT_NAME);

        if (curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, static_cast<void*>(&captureBuffer)) != CURLE_OK)
            throw makeStringExceptionV(-1, "%s Access token request: Could not set 'CURLOPT_WRITEDATA' option!", COMPONENT_NAME);

        if (curl_easy_setopt(curlHandle, CURLOPT_USERAGENT, "HPCC Systems Log Access client") != CURLE_OK)
            throw makeStringExceptionV(-1, "%s Access token request: Could not set 'CURLOPT_USERAGENT' option!", COMPONENT_NAME);

        if (curl_easy_setopt(curlHandle, CURLOPT_FAILONERROR, 0L) != CURLE_OK) // Do not treat non-ok HTTP codes as an error
            throw makeStringExceptionV(-1, "%s Access token request: Could not set 'CURLOPT_FAILONERROR' option!", COMPONENT_NAME);

        try
        {
            curlResponseCode = curl_easy_perform(curlHandle);
        }
        catch (...)
        {
            throw makeStringExceptionV(-1, "%s Access token request: Unknown error!", COMPONENT_NAME);
        }

        StringBuffer tokenReqResponse;
        if (captureBuffer.length() > 0)
        {
            tokenReqResponse.append(captureBuffer.length(), (const char *)captureBuffer.toByteArray());
        }

        if (curlResponseCode == CURLE_OK) // this is not the same as http success
        {
            long httpResponseCode;
            curl_easy_getinfo(curlHandle, CURLINFO_RESPONSE_CODE, &httpResponseCode);

            if(httpResponseCode == 200L) // HTTP OK
            {
                if (!tokenReqResponse.isEmpty())
                {
                    try
                    {
                        /*Expected response format
                        {  "ext_expires_in": "3599", "expires_on": "1653408922", "access_token": "XYZ",
                            "expires_in": "3599", "not_before": "1653405022", "token_type": "Bearer",
                            "resource": "https://api.loganalytics.io"
                        }*/

                        Owned<IPropertyTree> tokenResponse = createPTreeFromJSONString(tokenReqResponse.str());
                        if (tokenResponse->hasProp("access_token"))
                        {
                            DBGLOG("%s: Azure Log Analytics API access Bearer token generated! Expires in '%s' ", COMPONENT_NAME, nullText(tokenResponse->queryProp("expires_in")));
                            token.set(tokenResponse->queryProp("access_token"));
                        }
                        else
                        {
                            throw makeStringExceptionV(-1, "Received invalid token request response: '%s'",  tokenReqResponse.str());
                        }
                    }
                    catch(const std::exception& e)
                    {
                        throw makeStringExceptionV(-1, "%s Could not parse response for Azure Log Analytics API access token request: %s", COMPONENT_NAME, e.what());
                    }
                }
                else
                {
                    throw makeStringExceptionV(-1, "%s Access token request: Empty response received! This could indicate a problem with the request or the server", COMPONENT_NAME);
                }
            }
            else
            {
                /*
                    expected error response layout:
                    {
                        "error": "invalid_client",
                        "error_description": "AADSTS7000215: Invalid client secret is provided."
                    }
                */

                if (tokenReqResponse.isEmpty())
                    tokenReqResponse.set("Unknown error");

                switch (httpResponseCode)
                {
                    case 400L:
                        throw makeStringExceptionV(-1,"%s Access token request: Error (400): Bad Request - Request is badly formed and failed (permanently): '%s'", COMPONENT_NAME, tokenReqResponse.str());
                    case 401L:
                        throw makeStringExceptionV(-1,"%s Access token request: Error (401): Unauthorized - Client needs to authenticate first: '%s'", COMPONENT_NAME, tokenReqResponse.str());
                    case 403L:
                        throw makeStringExceptionV(-1,"%s Access token request: Error (403): Forbidden - Client request is denied: '%s'", COMPONENT_NAME, tokenReqResponse.str());
                    case 404L:
                        throw makeStringExceptionV(-1,"%s Access token request: Error (404): NotFound - Request references a non-existing entity. Ensure configured tenantID is valid!: '%s'", COMPONENT_NAME, tokenReqResponse.str());
                    case 408L:
                        throw makeStringExceptionV(-1,"%s Access token request: Error (408): Timeout - Request has timed out: '%s'", COMPONENT_NAME, tokenReqResponse.str());
                    case 500L:
                        throw makeStringExceptionV(-1,"%s Access token request: Error (500): Internal Server Error - Azure AD service found an error while processing the request: '%s'", COMPONENT_NAME, tokenReqResponse.str());
                    case 502L:
                    case 503L:
                    case 504L:
                        throw makeStringExceptionV(-1,"%s Access token request: Error (%ld): Gateway/Service Unavailable/Timeout - Temporary Azure AD or network issues: '%s'", COMPONENT_NAME, httpResponseCode, tokenReqResponse.str());
                    default:
                        throw makeStringExceptionV(-1,"%s Access token request: Error (%ld): '%s'", COMPONENT_NAME, httpResponseCode, tokenReqResponse.str());
                }
            }
        }
        else
        {
            throw makeStringExceptionV(-1, "%s Access token request: CURL ACTION FAILED! '%s'", COMPONENT_NAME, curl_easy_strerror(curlResponseCode));
        }
    }
}

size_t stringCallback(char *contents, size_t size, size_t nmemb, void *userp)
{
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

static void submitKQLQuery(std::string & readBuffer, const char * token, const char * kql, const char * workspaceID, const char * timeSpan)
{
    if (isEmptyString(token))
        throw makeStringExceptionV(-1, "%s KQL request: Empty LogAnalytics Workspace Token detected!", COMPONENT_NAME);

    if (isEmptyString(kql))
        throw makeStringExceptionV(-1, "%s KQL request: Empty KQL query detected!", COMPONENT_NAME);

    if (isEmptyString(workspaceID))
        throw makeStringExceptionV(-1, "%s KQL request: Empty WorkspaceID detected!", COMPONENT_NAME);

    if (isEmptyString(timeSpan))
        throw makeStringExceptionV(-1, "%s KQL request: Empty timeSpan detected!", COMPONENT_NAME);

    OwnedPtrCustomFree<CURL, curl_easy_cleanup> curlHandle = curl_easy_init();
    if (curlHandle)
    {
        CURLcode                curlResponseCode;
        OwnedPtrCustomFree<curl_slist, curl_slist_free_all> headers = nullptr;
        char                    curlErrBuffer[CURL_ERROR_SIZE];
        curlErrBuffer[0] = '\0';

        char * encodedKQL = curl_easy_escape(curlHandle, kql, strlen(kql));
        char * encodedTimeSpan = curl_easy_escape(curlHandle, timeSpan, strlen(timeSpan));
        VStringBuffer kqlQueryString("https://api.loganalytics.azure.com/v1/workspaces/%s/query?query=%s&timespan=%s", workspaceID, encodedKQL, encodedTimeSpan);
        curl_free(encodedTimeSpan);
        curl_free(encodedKQL);

        DBGLOG("%s: Full ALA API query request: '%s'", COMPONENT_NAME, kqlQueryString.str());

        VStringBuffer bearerHeader("Authorization: Bearer %s", token);

        /*curl -X GET 
        -H "Authorization: Bearer <TOKEN>"
            "https://api.loganalytics.azure.com/v1/workspaces/<workspaceID>/query?query=ContainerLog20%7C%20limit%20100&timespan=2022-05-11T06:45:00.000Z%2F2022-05-11T13:00:00.000Z"
        */

        headers = curl_slist_append(headers, bearerHeader.str());

        if (curl_easy_setopt(curlHandle, CURLOPT_HTTPHEADER, headers.getClear()) != CURLE_OK)
            throw makeStringExceptionV(-1, "%s: Log query request: Could not set 'CURLOPT_HTTPHEADER'", COMPONENT_NAME);

        if (curl_easy_setopt(curlHandle, CURLOPT_URL, kqlQueryString.str()) != CURLE_OK)
            throw makeStringExceptionV(-1, "%s: Log query request: Could not set 'CURLOPT_URL' (%s)!", COMPONENT_NAME, kqlQueryString.str());

        if (curl_easy_setopt(curlHandle, CURLOPT_POST, 0) != CURLE_OK)
            throw makeStringExceptionV(-1, "%s: Log query request: Could not disable 'CURLOPT_POST' option!", COMPONENT_NAME);

        if (curl_easy_setopt(curlHandle, CURLOPT_HTTPGET, 1) != CURLE_OK)
            throw makeStringExceptionV(-1, "%s: Log query request: Could not set 'CURLOPT_HTTPGET' option!", COMPONENT_NAME);

        if (curl_easy_setopt(curlHandle, CURLOPT_NOPROGRESS, 1) != CURLE_OK)
            throw makeStringExceptionV(-1, "%s: Log query request: Could not set 'CURLOPT_NOPROGRESS' option!", COMPONENT_NAME);

        if (curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, stringCallback) != CURLE_OK)
            throw makeStringExceptionV(-1, "%s: Log query request: Could not set 'CURLOPT_WRITEFUNCTION' option!", COMPONENT_NAME);

        if (curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, &readBuffer) != CURLE_OK)
            throw makeStringExceptionV(-1, "%s: Log query request: Could not set 'CURLOPT_WRITEDATA' option!", COMPONENT_NAME);

        if (curl_easy_setopt(curlHandle, CURLOPT_USERAGENT, "HPCC Systems Log Access client") != CURLE_OK)
            throw makeStringExceptionV(-1, "%s: Log query request: Could not set 'CURLOPT_USERAGENT' option!", COMPONENT_NAME);

        if (curl_easy_setopt(curlHandle, CURLOPT_ERRORBUFFER, curlErrBuffer) != CURLE_OK)
            throw makeStringExceptionV(-1, "%s: Log query request: Could not set 'CURLOPT_ERRORBUFFER' option!", COMPONENT_NAME);

        if (curl_easy_setopt(curlHandle, CURLOPT_FAILONERROR, 1L) != CURLE_OK) // non HTTP Success treated as error
            throw makeStringExceptionV(-1, "%s: Log query request: Could not set 'CURLOPT_FAILONERROR'option!", COMPONENT_NAME);

        try
        {
            curlResponseCode = curl_easy_perform(curlHandle);
        }
        catch (...)
        {
            throw makeStringExceptionV(-1, "%s KQL request: Unknown libcurl error", COMPONENT_NAME);
        }

        if (curlResponseCode != CURLE_OK)
        {
            long response_code;
            curl_easy_getinfo(curlHandle, CURLINFO_RESPONSE_CODE, &response_code);

            StringBuffer message;
            switch (response_code)
            {
            case 400L:
                throw makeStringExceptionV(-1,"%s KQL response: Error (400): Request is badly formed and failed (permanently): '%s'", COMPONENT_NAME, curlErrBuffer);
            case 401L:
                throw makeStringExceptionV(-1,"%s KQL response: Error (401): Unauthorized - Client needs to authenticate first: '%s'", COMPONENT_NAME, curlErrBuffer);
            case 403L:
                throw makeStringExceptionV(-1,"%s KQL response: Error (403): Forbidden - Client request is denied: '%s'", COMPONENT_NAME, curlErrBuffer);
            case 404L:
                throw makeStringExceptionV(-1,"%s KQL request: Error (404): NotFound - Request references a non-existing entity. Ensure configured WorkspaceID is valid!: '%s'", COMPONENT_NAME, curlErrBuffer);
            case 413:
                throw makeStringExceptionV(-1,"%s KQL request: Error (413): PayloadTooLarge - Request payload exceeded limits: '%s'", COMPONENT_NAME, curlErrBuffer);
            case 429:
                throw makeStringExceptionV(-1,"%s KQL request: Error (429): TooManyRequests - Request has been denied because of throttling: '%s'", COMPONENT_NAME, curlErrBuffer);
            case 504:
                throw makeStringExceptionV(-1,"%s KQL request: Error (504): Timeout - Request has timed out: '%s'", COMPONENT_NAME, (curlErrBuffer[0] ? curlErrBuffer : "" ));
            case 520:
                throw makeStringExceptionV(-1,"%s KQL request: Error (520): Azure ServiceError - Service found an error while processing the request: '%s'", COMPONENT_NAME, curlErrBuffer);
            default:
                throw makeStringExceptionV(-1,"%s KQL request: Error (%d): '%s'", COMPONENT_NAME, curlResponseCode, (curlErrBuffer[0] ? curlErrBuffer : "Unknown Error"));
            }
        }
        else if (readBuffer.length() == 0)
            throw makeStringExceptionV(-1, "%s KQL request: Empty response!", COMPONENT_NAME);
    }
}

static constexpr const char * azureLogAccessSecretCategory = "esp";
static constexpr const char * azureLogAccessSecretName = "azure-logaccess";

static constexpr const char * azureLogAccessSecretAADTenantID = "aad-tenant-id";
static constexpr const char * azureLogAccessSecretAADClientID = "aad-client-id";
static constexpr const char * azureLogAccessSecretAADClientSecret = "aad-client-secret";
static constexpr const char * azureLogAccessSecretWorkspaceID = "ala-workspace-id";

AzureLogAnalyticsCurlClient::AzureLogAnalyticsCurlClient(IPropertyTree & logAccessPluginConfig)
{
    PROGLOG("%s: Resolving all required configuration values...", COMPONENT_NAME);

    Owned<const IPropertyTree> secretTree = getSecret(azureLogAccessSecretCategory, azureLogAccessSecretName);
    if (!secretTree)
        throw makeStringExceptionV(-1, "%s: Could not fetch %s information!", COMPONENT_NAME, azureLogAccessSecretName);

    getSecretKeyValue(m_aadTenantID.clear(), secretTree, azureLogAccessSecretAADTenantID);
    if (m_aadTenantID.isEmpty())
    {
        WARNLOG("%s: Could not find '%s.%s' secret value!", COMPONENT_NAME, azureLogAccessSecretName, azureLogAccessSecretAADTenantID);
        m_aadTenantID.set(logAccessPluginConfig.queryProp("connection/@tenantID"));
        if (m_aadTenantID.isEmpty())
            throw makeStringExceptionV(-1, "%s: Could not find AAD Tenant ID, provide it as part of '%s.%s' secret, or connection/@tenantID in AzureClient LogAccess configuration!", COMPONENT_NAME, azureLogAccessSecretName, azureLogAccessSecretAADTenantID);
    }

    getSecretKeyValue(m_aadClientID.clear(), secretTree, azureLogAccessSecretAADClientID);
    if (m_aadClientID.isEmpty())
    {
        WARNLOG("%s: Could not find '%s.%s' secret value!", COMPONENT_NAME, azureLogAccessSecretName, azureLogAccessSecretAADClientID);
        m_aadClientID.set(logAccessPluginConfig.queryProp("connection/@clientID"));

        if (m_aadClientID.isEmpty())
            throw makeStringExceptionV(-1, "%s: Could not find AAD Client ID, provide it as part of %s.%s secret, or connection/@clientID in AzureClient LogAccess configuration!", COMPONENT_NAME, azureLogAccessSecretName, azureLogAccessSecretAADClientID);
    }

    getSecretKeyValue(m_aadClientSecret.clear(),secretTree, azureLogAccessSecretAADClientSecret);
    if (m_aadClientSecret.isEmpty())
    {
        throw makeStringExceptionV(-1, "%s: Required secret '%s.%s' not found!", COMPONENT_NAME, azureLogAccessSecretName, azureLogAccessSecretAADClientSecret);
    }

    getSecretKeyValue(m_logAnalyticsWorkspaceID.clear(), secretTree, azureLogAccessSecretWorkspaceID);
    if (m_logAnalyticsWorkspaceID.isEmpty())
    {
        WARNLOG("%s: Could not find '%s.%s' secret value!", COMPONENT_NAME, azureLogAccessSecretName, azureLogAccessSecretWorkspaceID);
        m_logAnalyticsWorkspaceID.set(logAccessPluginConfig.queryProp("connection/@workspaceID"));

        if (m_logAnalyticsWorkspaceID.isEmpty())
            throw makeStringExceptionV(-1, "%s: Could not find ALA Workspace ID, provide it as part of %s.%s secret, or connection/@workspaceID in AzureClient LogAccess configuration!", COMPONENT_NAME, azureLogAccessSecretName, azureLogAccessSecretWorkspaceID);
    }

    m_pluginCfg.set(&logAccessPluginConfig);

    //blobMode is a flag to determine if the log entry is to be parsed or not
    m_blobMode = logAccessPluginConfig.getPropBool("@blobMode", false);
    DBGLOG("%s: Blob Mode: %s", COMPONENT_NAME, m_blobMode ? "Enabled" : "Disabled");

    m_globalIndexTimestampField.set(defaultHPCCLogTimeStampCol);
    m_globalIndexSearchPattern.set(defaultIndexPattern);
    m_globalSearchColName.set(defaultHPCCLogMessageCol);

    m_classSearchColName.set(defaultHPCCLogTypeCol);
    m_workunitSearchColName.set(defaultHPCCLogJobIDCol);
    m_componentsSearchColName.set(defaultHPCCLogComponentCol);
    m_audienceSearchColName.set(defaultHPCCLogAudCol);
    m_traceSearchColName.set(defaultHPCCLogTraceIDCol);
    m_spanSearchColName.set(defaultHPCCLogSpanIDCol);

    Owned<IPropertyTreeIterator> logMapIter = m_pluginCfg->getElements("logMaps");
    ForEach(*logMapIter)
    {
        IPropertyTree & logMap = logMapIter->query();
        const char * logMapType = logMap.queryProp("@type");
        if (streq(logMapType, "global"))
        {
            if (logMap.hasProp(logMapIndexPatternAtt))
            {
                m_globalIndexSearchPattern = logMap.queryProp(logMapIndexPatternAtt);
                targetIsContainerLogV2 = strcmp("ContainerLogV2", m_globalIndexSearchPattern)==0;
            }
            if (logMap.hasProp(logMapSearchColAtt))
                m_globalSearchColName = logMap.queryProp(logMapSearchColAtt);
            if (logMap.hasProp(logMapTimeStampColAtt))
                m_globalIndexTimestampField = logMap.queryProp(logMapTimeStampColAtt);
        }
        else if (streq(logMapType, "workunits"))
        {
            if (logMap.hasProp(logMapIndexPatternAtt))
                m_workunitIndexSearchPattern = logMap.queryProp(logMapIndexPatternAtt);
            if (logMap.hasProp(logMapSearchColAtt))
                m_workunitSearchColName = logMap.queryProp(logMapSearchColAtt);
        }
        else if (streq(logMapType, "components"))
        {
            if (logMap.hasProp(logMapIndexPatternAtt))
                m_componentsIndexSearchPattern = logMap.queryProp(logMapIndexPatternAtt);
            if (logMap.hasProp(logMapSearchColAtt))
                m_componentsSearchColName = logMap.queryProp(logMapSearchColAtt);
            if (logMap.hasProp(logMapKeyColAtt))
                m_componentsLookupKeyColumn = logMap.queryProp(logMapKeyColAtt);
            if (logMap.hasProp(logMapTimeStampColAtt))
                m_componentsTimestampField = logMap.queryProp(logMapTimeStampColAtt);
            else
                m_componentsTimestampField = defaultHPCCLogComponentTSCol;

            m_disableComponentNameJoins = logMap.getPropBool(logMapDisableJoinsAtt, false);
            if (targetIsContainerLogV2)
                m_disableComponentNameJoins = true; //Don't attempt a join on ContainerLogV2
            else
            {
                if (strcmp("ContainerLogV2", m_componentsIndexSearchPattern)==0)
                    targetIsContainerLogV2 = true;

                m_disableComponentNameJoins =  !m_disableComponentNameJoins && logMap.getPropBool(logMapDisableJoinsAtt, false);
            }
        }
        else if (streq(logMapType, "class"))
        {
            if (logMap.hasProp(logMapIndexPatternAtt))
                m_classIndexSearchPattern = logMap.queryProp(logMapIndexPatternAtt);
            if (logMap.hasProp(logMapSearchColAtt))
                m_classSearchColName = logMap.queryProp(logMapSearchColAtt);
        }
        else if (streq(logMapType, "audience"))
        {
            if (logMap.hasProp(logMapIndexPatternAtt))
                m_audienceIndexSearchPattern = logMap.queryProp(logMapIndexPatternAtt);
            if (logMap.hasProp(logMapSearchColAtt))
                m_audienceSearchColName = logMap.queryProp(logMapSearchColAtt);
        }
        else if (streq(logMapType, "instance"))
        {
            if (logMap.hasProp(logMapIndexPatternAtt))
                m_instanceIndexSearchPattern = logMap.queryProp(logMapIndexPatternAtt);
            if (logMap.hasProp(logMapSearchColAtt))
                m_instanceSearchColName = logMap.queryProp(logMapSearchColAtt);
            if (logMap.hasProp(logMapKeyColAtt))
                m_instanceLookupKeyColumn = logMap.queryProp(logMapKeyColAtt);
        }
        else if (streq(logMapType, "node"))
        {
            if (logMap.hasProp(logMapIndexPatternAtt))
                m_hostIndexSearchPattern = logMap.queryProp(logMapIndexPatternAtt);
            if (logMap.hasProp(logMapSearchColAtt))
                m_hostSearchColName = logMap.queryProp(logMapSearchColAtt);
        }
        else if (streq(logMapType, "host"))
        {
            OWARNLOG("%s: 'host' LogMap entry is deprecated and replaced by 'node'!", COMPONENT_NAME);
            if (isEmptyString(m_hostIndexSearchPattern) && isEmptyString(m_hostSearchColName))
            {
                if (logMap.hasProp(logMapIndexPatternAtt))
                    m_hostIndexSearchPattern = logMap.queryProp(logMapIndexPatternAtt);
                if (logMap.hasProp(logMapSearchColAtt))
                    m_hostSearchColName = logMap.queryProp(logMapSearchColAtt);
            }
            else
                OERRLOG("%s: Possible LogMap collision detected, 'host' and 'node' refer to same log column!", COMPONENT_NAME); 
        }
        else if (streq(logMapType, "pod"))
        {
            if (logMap.hasProp(logMapIndexPatternAtt))
                m_podIndexSearchPattern = logMap.queryProp(logMapIndexPatternAtt);
            if (logMap.hasProp(logMapSearchColAtt))
                m_podSearchColName = logMap.queryProp(logMapSearchColAtt);
        }
        else if (streq(logMapType, "traceid"))
        {
            if (logMap.hasProp(logMapIndexPatternAtt))
                m_traceIndexSearchPattern = logMap.queryProp(logMapIndexPatternAtt);

            if (logMap.hasProp(logMapSearchColAtt))
                m_traceSearchColName.set(logMap.queryProp(logMapSearchColAtt));
        }
        else if (streq(logMapType, "spanid"))
        {
            if (logMap.hasProp(logMapIndexPatternAtt))
                m_spanIndexSearchPattern = logMap.queryProp(logMapIndexPatternAtt);

            if (logMap.hasProp(logMapSearchColAtt))
                m_spanSearchColName.set(logMap.queryProp(logMapSearchColAtt));
        }
        else
        {
            ERRLOG("Encountered invalid LogAccess field map type: '%s'", logMapType);
        }
    }
}

void AzureLogAnalyticsCurlClient::getMinReturnColumns(StringBuffer & columns, const bool includeComponentName)
{
    columns.append("\n| project ");
    if (includeComponentName)
    {
        if (targetIsContainerLogV2 && m_componentsSearchColName.length() > 0)
        {
            columns.append(m_componentsSearchColName.str());
            if (m_componentsLookupKeyColumn.length() > 0 && !strsame(m_componentsSearchColName.str(), m_componentsLookupKeyColumn.str()))
                columns.appendf("=%s", m_componentsLookupKeyColumn.str());
        }
        else
            columns.append(defaultHPCCLogComponentCol);
        columns.append(", ");
    }

    if (m_blobMode)
        columns.appendf("%s, %s", m_globalIndexTimestampField.str(), m_globalSearchColName.str());
    else
        columns.appendf("%s, %s", m_globalIndexTimestampField.str(), defaultHPCCLogMessageCol);
}

void AzureLogAnalyticsCurlClient::getDefaultReturnColumns(StringBuffer & columns, const bool includeComponentName)
{
    columns.append("\n| project ");

    if (includeComponentName)
    {
        if (targetIsContainerLogV2 && m_componentsSearchColName.length() > 0)
        {
            columns.append(m_componentsSearchColName.str());
            if (m_componentsLookupKeyColumn.length() > 0 && !strsame(m_componentsSearchColName.str(), m_componentsLookupKeyColumn.str()))
                columns.appendf("=%s", m_componentsLookupKeyColumn.str());
        }
        else
            columns.append(defaultHPCCLogComponentCol);

        columns.append(", ");
    }

    if (!isEmptyString(m_instanceSearchColName.str()))
    {
        columns.appendf("%s", m_instanceSearchColName.str());

        if (m_instanceLookupKeyColumn.length()>0 && !strsame(m_instanceLookupKeyColumn.str(),m_instanceSearchColName.str()))
            columns.appendf("=%s", m_instanceLookupKeyColumn.str());

        columns.append(", ");
    }

    if (!isEmptyString(m_podSearchColName))
        columns.appendf("%s, ", m_podSearchColName.str());

    if (m_blobMode)
        columns.appendf("%s, %s", m_globalIndexTimestampField.str(), m_globalSearchColName.str());
    else
        columns.appendf("%s, %s, %s, %s, %s, %s, %s, %s, %s, %s",
            m_globalIndexTimestampField.str(), defaultHPCCLogMessageCol, m_classSearchColName.str(),
            m_audienceSearchColName.str(), m_workunitSearchColName.str(), m_traceSearchColName.str(), m_spanSearchColName.str(), defaultHPCCLogSeqCol, defaultHPCCLogThreadIDCol, defaultHPCCLogProcIDCol);
}

bool generateHPCCLogColumnstAllColumns(StringBuffer & kql, const char * colName, bool targetsV2, bool blobMode)
{
    if (isEmptyString(colName))
    {
        ERRLOG("%s Cannot generate HPCC Log Columns, empty source column detected", COMPONENT_NAME);
        return false;
    }

    StringBuffer sourceCol;
    if (targetsV2 && strcmp(colName, "LogMessage")==0)
        sourceCol.set("tostring(LogMessage)");
    else if (!targetsV2 && strcmp(colName, "LogEntry")==0)
        sourceCol.append(colName);
    else
        throw makeStringExceptionV(-1, "%s: Invalid Azure Log Analytics log message column name detected: '%s'. Review logAccess configuration.", COMPONENT_NAME, colName);

    if (!blobMode)
    {
        kql.appendf("\n| extend hpcclogfields = extract_all(@\'^([0-9A-Fa-f]+)\\s+(OPR|USR|PRG|AUD|UNK)\\s+(DIS|ERR|WRN|INF|PRO|MET|UNK)\\s+(\\d{4}-\\d{2}-\\d{2}\\s\\d{2}:\\d{2}:\\d{2}\\.\\d+)\\s+(\\d+)\\s+(\\d+)\\s+(UNK|[A-Z]\\d{8}-\\d{6}(?:-\\d+)?)\\s*([0-9a-fA-F]{32}|UNK)?\\s*([0-9a-fA-F]{16}|UNK)?\\s+)?\\\"(.*)\\\"$', %s)[0]", sourceCol.str());
        kql.appendf("\n| extend %s = tostring(hpcclogfields.[0])", defaultHPCCLogSeqCol);
        kql.appendf("\n| extend %s = tostring(hpcclogfields.[1])", defaultHPCCLogAudCol);
        kql.appendf("\n| extend %s = tostring(hpcclogfields.[2])", defaultHPCCLogTypeCol);
        kql.appendf("\n| extend %s = todatetime(hpcclogfields.[3])", defaultHPCCLogTimeStampCol);
        kql.appendf("\n| extend %s = toint(hpcclogfields.[4])", defaultHPCCLogProcIDCol);
        kql.appendf("\n| extend %s = toint(hpcclogfields.[5])",  defaultHPCCLogThreadIDCol);
        kql.appendf("\n| extend %s = tostring(hpcclogfields.[6])", defaultHPCCLogJobIDCol);
        kql.appendf("\n| extend %s = tostring(hpcclogfields.[7])", defaultHPCCLogTraceIDCol);
        kql.appendf("\n| extend %s = tostring(hpcclogfields.[8])", defaultHPCCLogSpanIDCol);
        kql.appendf("\n| extend %s = tostring(hpcclogfields.[9])", defaultHPCCLogMessageCol);

        kql.appendf("\n| project-away hpcclogfields, Type, TenantId, _ResourceId, %s, ", colName);
    }
    else
    {
        kql.appendf("\n| project-away Type, TenantId, _ResourceId, ");
    }

    if (targetsV2)
        kql.append("LogSource, SourceSystem");
    else
        kql.append("LogEntrySource, TimeOfCommand, SourceSystem");

    return true;
}

void AzureLogAnalyticsCurlClient::searchMetaData(StringBuffer & search, const LogAccessReturnColsMode retcolmode, const StringArray & selectcols, bool & includeComponentName, unsigned size, offset_t from)
{
    switch (retcolmode)
    {
    case RETURNCOLS_MODE_all:
        // in KQL, no project ALL supported
        includeComponentName = true;
        break;
    case RETURNCOLS_MODE_min:
        getMinReturnColumns(search, includeComponentName);
        break;
    case RETURNCOLS_MODE_default:
        getDefaultReturnColumns(search, includeComponentName);
        break;
    case RETURNCOLS_MODE_custom:
    {
        if (selectcols.length() > 0)
        {
            includeComponentName = false;
            search.append("\n| project ");
            selectcols.getString(search, ",");

            //determine if componentname field included or not
            if (!isEmptyString(m_componentsSearchColName))
            {
                if (selectcols.contains(defaultHPCCLogComponentCol))
                {
                    includeComponentName = true;
                    break;
                }
            }
        }
        else
        {
            throw makeStringExceptionV(-1, "%s: Custom return columns specified, but no columns provided", COMPONENT_NAME);
        }
        break;
    }
    default:
        throw makeStringExceptionV(-1, "%s: Could not determine return colums mode", COMPONENT_NAME);
    }

    //currently setting default behaviour to sort by timestamp in ascending manner, in future this should be configurable
    search.appendf("\n| sort by %s asc", m_globalIndexTimestampField.str());
    /* KQL doesn't support offset, we could serialize, and assign row numbers but this is expensive
    | serialize
    | extend _row=row_number()
    | where _row > 20
    */
    search.appendf("\n| limit %s", std::to_string(size).c_str());
}

void AzureLogAnalyticsCurlClient::azureLogAnalyticsQueryTimeSpanString(StringBuffer & queryTimeSpan, std::time_t from, std::time_t to)
{
    if (from == -1)
        throw makeStringExceptionV(-1, "%s: Invalid 'from' timestamp detected", COMPONENT_NAME);

    char fromTimeStr[40];
    std::strftime(fromTimeStr, sizeof(fromTimeStr), "%Y-%m-%dT%H:%M:%S", std::gmtime(&from));
    queryTimeSpan.set(fromTimeStr);

    if (to != -1)
    {
        char toTimeStr[40];
        std::strftime(toTimeStr, sizeof(toTimeStr), "%Y-%m-%dT%H:%M:%S", std::gmtime(&to));
        queryTimeSpan.appendf("/%s", toTimeStr);
    }
}

void AzureLogAnalyticsCurlClient::azureLogAnalyticsTimestampQueryRangeString(StringBuffer & range, const char * timeStampField, std::time_t from, std::time_t to)
{
    if (isEmptyString(timeStampField))
        throw makeStringExceptionV(-1, "%s: TimeStamp Field must be provided", COMPONENT_NAME);

    //let startDateTime = datetime('2022-05-11T06:45:00.000Z');
    //let endDateTime = datetime('2022-05-11T13:00:00.000Z');
    //| where TimeGenerated >= startDateTime and TimeGenerated < endDateTime
    range.setf("%s >= unixtime_milliseconds_todatetime(%s)", timeStampField, std::to_string(from*1000).c_str());
    if (to != -1) //aka 'to' has been initialized
    {
        range.appendf(" and %s < unixtime_milliseconds_todatetime(%s)", timeStampField, std::to_string(to*1000).c_str());
    }
}

void throwIfMultiIndexDetected(const char * currentIndex, const char * proposedIndex)
{
    if (!isEmptyString(currentIndex) && !strsame(currentIndex,proposedIndex))
        throw makeStringExceptionV(-1, "%s: Multi-index query not supported: '%s' - '%s'", COMPONENT_NAME, currentIndex, proposedIndex);
}

void AzureLogAnalyticsCurlClient::populateKQLQueryString(StringBuffer & queryString, StringBuffer & queryIndex, const ILogAccessFilter * filter)
{
    if (filter == nullptr)
        throw makeStringExceptionV(-1, "%s: Null filter detected while creating Azure KQL query string", COMPONENT_NAME);

    StringBuffer queryValue;
    std::string queryField = m_globalSearchColName.str();
    std::string queryOperator = " =~ ";

    if (m_blobMode)
    {
        queryOperator = " has ";
    }

    filter->toString(queryValue);
    switch (filter->filterType())
    {
    case LOGACCESS_FILTER_jobid:
    {
        if (m_workunitSearchColName.isEmpty())
            throw makeStringExceptionV(-1, "%s: 'JobID' log entry field not configured", COMPONENT_NAME);

        queryField = m_workunitSearchColName.str();

        if (!m_workunitIndexSearchPattern.isEmpty())
        {
            throwIfMultiIndexDetected(queryIndex.str(), m_workunitIndexSearchPattern.str());
            queryIndex = m_workunitIndexSearchPattern;
        }

        DBGLOG("%s: Searching log entries by jobid: '%s'...", COMPONENT_NAME, queryValue.str());
        break;
    }
    case LOGACCESS_FILTER_class:
    {
        if (m_classSearchColName.isEmpty())
            throw makeStringExceptionV(-1, "%s: 'Class' log entry field not configured", COMPONENT_NAME);

        queryField = m_classSearchColName.str();

        if (!m_classIndexSearchPattern.isEmpty())
        {
            throwIfMultiIndexDetected(queryIndex.str(), m_classIndexSearchPattern.str());
            queryIndex = m_classIndexSearchPattern.str();
        }

        DBGLOG("%s: Searching log entries by class: '%s'...", COMPONENT_NAME, queryValue.str());
        break;
    }
    case LOGACCESS_FILTER_trace:
    {
        if (m_traceSearchColName.isEmpty())
            throw makeStringExceptionV(-1, "%s: 'Trace' log entry field not configured", COMPONENT_NAME);

        queryField = m_traceSearchColName.str();

        if (!m_traceIndexSearchPattern.isEmpty())
        {
            throwIfMultiIndexDetected(queryIndex.str(), m_traceIndexSearchPattern.str());
            queryIndex = m_traceIndexSearchPattern.str();
        }

        DBGLOG("%s: Searching log entries by traceid: '%s'...", COMPONENT_NAME, queryValue.str());
        break;
    }
    case LOGACCESS_FILTER_span:
    {
        if (m_spanSearchColName.isEmpty())
            throw makeStringExceptionV(-1, "%s: 'Span' log entry field not configured", COMPONENT_NAME);

        queryField = m_spanSearchColName.str();

        if (!m_spanIndexSearchPattern.isEmpty())
        {
            throwIfMultiIndexDetected(queryIndex.str(), m_spanIndexSearchPattern.str());
            queryIndex = m_spanIndexSearchPattern.str();
        }

        DBGLOG("%s: Searching log entries by spanid: '%s'...", COMPONENT_NAME, queryValue.str());
        break;
    }
    case LOGACCESS_FILTER_audience:
    {
        if (m_audienceSearchColName.isEmpty())
            throw makeStringExceptionV(-1, "%s: 'Audience' log entry field not configured", COMPONENT_NAME);
        
        queryField = m_audienceSearchColName.str();

        if (!m_audienceIndexSearchPattern.isEmpty())
        {
           throwIfMultiIndexDetected(queryIndex.str(), m_audienceIndexSearchPattern.str());
           queryIndex = m_audienceIndexSearchPattern.str();
        }

        DBGLOG("%s: Searching log entries by target audience: '%s'...", COMPONENT_NAME, queryValue.str());
        break;
    }
    case LOGACCESS_FILTER_component:
    {
        if (m_componentsSearchColName.isEmpty())
            throw makeStringExceptionV(-1, "%s: 'Host' log entry field not configured", COMPONENT_NAME);

        queryField = m_componentsSearchColName.str();

        if (!m_componentsIndexSearchPattern.isEmpty())
        {
           throwIfMultiIndexDetected(queryIndex.str(), m_componentsIndexSearchPattern.str());
           queryIndex = m_componentsIndexSearchPattern.str();
        }

        DBGLOG("%s: Searching '%s' component log entries...", COMPONENT_NAME, queryValue.str());
        break;
    }
    case LOGACCESS_FILTER_host:
    {
        if (m_hostSearchColName.isEmpty())
            throw makeStringExceptionV(-1, "%s: 'Host' log entry field not configured", COMPONENT_NAME);

        queryField = m_hostSearchColName.str();

        if (!m_hostIndexSearchPattern.isEmpty())
        {
            throwIfMultiIndexDetected(queryIndex.str(), m_hostIndexSearchPattern.str());
            queryIndex = m_hostIndexSearchPattern.str();
        }

        DBGLOG("%s: Searching log entries by host: '%s'", COMPONENT_NAME, queryValue.str());
        break;
    }
    case LOGACCESS_FILTER_instance:
    {
        if (m_instanceSearchColName.isEmpty())
            throw makeStringExceptionV(-1, "%s: 'Instance' log entry field not configured", COMPONENT_NAME);

        if (m_instanceLookupKeyColumn.length()>0 && !strsame(m_instanceLookupKeyColumn.str(),m_instanceSearchColName.str()))
            queryField = m_instanceLookupKeyColumn.str();
        else
            queryField = m_instanceSearchColName.str();

        if (!m_instanceIndexSearchPattern.isEmpty())
        {
            throwIfMultiIndexDetected(queryIndex.str(),  m_instanceIndexSearchPattern.str());
            queryIndex = m_instanceIndexSearchPattern.str();
        }

        DBGLOG("%s: Searching log entries by HPCC component instance: '%s'", COMPONENT_NAME, queryValue.str() );
        break;
    }
    case LOGACCESS_FILTER_wildcard:
        if (queryValue.isEmpty())
            throw makeStringExceptionV(-1, "%s: Wildcard filter cannot be empty!", COMPONENT_NAME);

        queryOperator = " contains ";
        DBGLOG("%s: Searching log entries by wildcard filter: '%s %s %s'...", COMPONENT_NAME, queryField.c_str(), queryOperator.c_str(), queryValue.str());

        break;
    case LOGACCESS_FILTER_or:
    case LOGACCESS_FILTER_and:
    {
        StringBuffer op(logAccessFilterTypeToString(filter->filterType()));
        queryString.append(" ( ");
        populateKQLQueryString(queryString, queryIndex, filter->leftFilterClause());
        queryString.append(" ");
        queryString.append(op.toLowerCase()); //KQL or | and
        queryString.append(" ");
        populateKQLQueryString(queryString, queryIndex, filter->rightFilterClause());
        queryString.append(" ) ");
        return; // queryString populated, need to break out
    }
    case LOGACCESS_FILTER_pod:
    {
        queryField = m_podSearchColName.str();

        if (!m_podIndexSearchPattern.isEmpty())
        {
            throwIfMultiIndexDetected(queryIndex.str(),  m_podIndexSearchPattern.str());
            queryIndex = m_podIndexSearchPattern.str();
        }

        DBGLOG("%s: Searching log entries by Pod: '%s'", COMPONENT_NAME, queryValue.str() );
        break;
    }
    case LOGACCESS_FILTER_column:
        if (filter->getFieldName() == nullptr)
            throw makeStringExceptionV(-1, "%s: empty field name detected in filter by column!", COMPONENT_NAME);
        queryField = filter->getFieldName();
        break;
    default:
        throw makeStringExceptionV(-1, "%s: Unknown query criteria type encountered: '%s'", COMPONENT_NAME, queryValue.str());
    }

    if (queryIndex.isEmpty())
        queryIndex = m_globalIndexSearchPattern.str();

    //KQL structure:
    //TableName
    //| where Fieldname OPERATOR 'value'
    queryString.append(" ").append(queryField.c_str()).append(queryOperator.c_str()).append("'").append(queryValue.str()).append("'");
}

void AzureLogAnalyticsCurlClient::declareContainerIndexJoinTable(StringBuffer & queryString, const LogAccessConditions & options)
{
    const LogAccessTimeRange & trange = options.getTimeRange();

    StringBuffer range;
    azureLogAnalyticsTimestampQueryRangeString(range, m_componentsTimestampField.str(), trange.getStartt().getSimple(),trange.getEndt().isNull() ? -1 : trange.getEndt().getSimple());

    queryString.append("let ").append(m_componentsIndexSearchPattern).append("Table = ").append(m_componentsIndexSearchPattern);
    queryString.append("\n| where ").append(range.str());
    queryString.append("\n| extend ").append(defaultHPCCLogComponentCol).append(" = extract(\"k8s_([0-9A-Za-z-]+)\", 1, ").append(m_componentsSearchColName).append(", typeof(string))");
    queryString.append("\n| project ").append(defaultHPCCLogComponentCol).append(", ").append(m_componentsLookupKeyColumn).append(";\n");
    
    queryString.append(m_componentsIndexSearchPattern).append("Table");
    queryString.append("\n| join\n( ");
}

void AzureLogAnalyticsCurlClient::populateKQLQueryString(StringBuffer & queryString, StringBuffer & queryIndex, const LogAccessConditions & options)
{
    try
    {
        const LogAccessTimeRange & trange = options.getTimeRange();
        if (trange.getStartt().isNull())
            throw makeStringExceptionV(-1, "%s: start time must be provided!", COMPONENT_NAME);

        //Forced to format log structure in query until a proper log ingest rule is created
        queryIndex.set(m_globalIndexSearchPattern.str());

        StringBuffer searchColumns;
        bool includeComponentName = !m_disableComponentNameJoins || targetIsContainerLogV2;
        searchMetaData(searchColumns, options.getReturnColsMode(), options.getLogFieldNames(), includeComponentName, options.getLimit(), options.getStartFrom());
        if (!m_disableComponentNameJoins && !targetIsContainerLogV2)
            declareContainerIndexJoinTable(queryString, options);

        queryString.append(queryIndex);
        //this used to parse m_globalSearchColName into hpcc.log.* fields, now just does a project-away
        generateHPCCLogColumnstAllColumns(queryString, m_globalSearchColName.str(), targetIsContainerLogV2, m_blobMode);

        if (options.queryFilter() == nullptr || options.queryFilter()->filterType() == LOGACCESS_FILTER_wildcard) // No filter
        {
            //no where clause
            queryIndex.set(m_globalIndexSearchPattern.str());
        }
        else
        {
            queryString.append("\n| where ");
            populateKQLQueryString(queryString, queryIndex, options.queryFilter());
        }

        StringBuffer range;
        azureLogAnalyticsTimestampQueryRangeString(range, m_globalIndexTimestampField.str(), trange.getStartt().getSimple(),trange.getEndt().isNull() ? -1 : trange.getEndt().getSimple());
        queryString.append("\n| where ").append(range.str());
        if (!m_disableComponentNameJoins && !targetIsContainerLogV2)
            queryString.append("\n) on ").append(m_componentsLookupKeyColumn);

        queryString.append(searchColumns);

        DBGLOG("%s: Search string '%s'", COMPONENT_NAME, queryString.str());
    }
    catch (std::runtime_error &e)
    {
        throw makeStringExceptionV(-1, "%s: Error populating KQL search string: %s", COMPONENT_NAME, e.what());
    }
    catch (IException * e)
    {
        StringBuffer mess;
        e->errorMessage(mess);
        e->Release();
        throw makeStringExceptionV(-1, "%s: Error populating KQL search string: %s", COMPONENT_NAME, mess.str());
    }
}

unsigned AzureLogAnalyticsCurlClient::processHitsJsonResp(IPropertyTreeIterator * lines, IPropertyTreeIterator * columns, StringBuffer & returnbuf, LogAccessLogFormat format, bool wrapped, bool reportHeader)
{
    if (!lines)
        throw makeStringExceptionV(-1, "%s: Detected null 'rows' Azure Log Analytics KQL response", COMPONENT_NAME);

    StringArray header;
    ForEach(*columns)
    {
        Owned<IPropertyTreeIterator> names = columns->query().getElements("name");
        ForEach(*names)
        {
            header.append(names->query().queryProp("."));
        }
    }

    unsigned recsProcessed = 0;
    switch (format)
    {
        case LOGACCESS_LOGFORMAT_xml:
        {
            if (wrapped)
                returnbuf.append("<lines>");

            ForEach(*lines)
            {
                returnbuf.append("<line>");
                Owned<IPropertyTreeIterator> fields = lines->query().getElements("rows");
                int idx = 0;
                ForEach(*fields)
                {
                    const char * fieldName = header.item(idx++);
                    const char * rawValue = fields->query().queryProp(".");

                    if (isEmptyString(rawValue))
                    {
                        returnbuf.appendf("<%s/>", fieldName);
                    }
                    else
                    {
                        StringBuffer encodedValue;
                        encodeXML(rawValue, encodedValue);
                        returnbuf.appendf("<%s>%s</%s>", fieldName, encodedValue.str(), fieldName);
                    }
                }
                returnbuf.append("</line>");
                recsProcessed++;
            }
            if (wrapped)
                returnbuf.append("</lines>");
            break;
        }
        case LOGACCESS_LOGFORMAT_json:
        {
            if (wrapped)
                returnbuf.append("{\"lines\": [");

            StringBuffer hitchildjson;
            bool firstLine = true;
            ForEach(*lines)
            {
                if (!firstLine)
                    returnbuf.append(",\n");

                Owned<IPropertyTreeIterator> fields = lines->query().getElements("rows");
                bool firstField = true;
                int idx = 0;
                ForEach(*fields)
                {
                    if (!firstField)
                        hitchildjson.append(", ");

                    StringBuffer encodedValue;
                    encodeJSON(encodedValue, fields->query().queryProp("."));

                    hitchildjson.appendf("\"%s\":\"%s\"", header.item(idx++), encodedValue.str());

                    firstField = false;
                }

                firstLine = false;
                returnbuf.appendf("{\"fields\": [ { %s } ]}", hitchildjson.str());
                hitchildjson.clear();
                recsProcessed++;
            }
            if (wrapped)
                returnbuf.append("]}");
            break;
        }
        case LOGACCESS_LOGFORMAT_csv:
        {
            if (reportHeader)
            {
                bool first = true;
                ForEachItemIn(idx, header)
                {
                    if (!first)
                        returnbuf.append(", ");
                    else
                        first = false;

                    returnbuf.append(header.item(idx));
                }
                returnbuf.newline();
            }

            //Process each column
            ForEach(*lines)
            {
                Owned<IPropertyTreeIterator> fieldElementsItr = lines->query().getElements("*");

                bool first = true;
                ForEach(*fieldElementsItr)
                {
                    if (!first)
                        returnbuf.append(", ");
                    else
                        first = false;

                    encodeCSVColumn(returnbuf, fieldElementsItr->query().queryProp("."));
                }
                returnbuf.newline();
                recsProcessed++;
            }
            break;
        }
        default:
            break;
    }
    return recsProcessed;
}

bool AzureLogAnalyticsCurlClient::processSearchJsonResp(LogQueryResultDetails & resultDetails, const std::string & retrievedDocument, StringBuffer & returnbuf, LogAccessLogFormat format, bool reportHeader)
{
    Owned<IPropertyTree> tree = createPTreeFromJSONString(retrievedDocument.c_str());
    if (!tree)
        throw makeStringExceptionV(-1, "%s: Could not parse query response", COMPONENT_NAME);

    resultDetails.totalReceived = processHitsJsonResp(tree->getElements("tables/rows"), tree->getElements("tables/columns"), returnbuf, format, true, reportHeader);
    resultDetails.totalAvailable = 0;
    return true;
}

void AzureLogAnalyticsCurlClient::healthReport(LogAccessHealthReportOptions options, LogAccessHealthReportDetails & report)
{
    LogAccessHealthStatus status = LOGACCESS_STATUS_success;
    try
    {
        StringBuffer configuration;

        if (m_pluginCfg)
        {
            if (options.IncludeConfiguration)
            {
                if (options.IncludeConfiguration)
                    toJSON(m_pluginCfg, configuration, 0, JSON_Format);
            }
        }
        else
        {
            status.escalateStatusCode(LOGACCESS_STATUS_fail);
            status.appendMessage("ALA Plug-in Configuration tree is empty!!!");
        }

        report.Configuration.set(configuration.str()); //empty if !(options.IncludeConfiguration)

        if (m_logAnalyticsWorkspaceID.length() == 0)
        {
            status.appendMessage("Target Azure Log Analytics workspace ID is empty!");
            status.escalateStatusCode(LOGACCESS_STATUS_fail);
        }

        if (m_aadTenantID.length() == 0)
        {
            status.appendMessage("Target Azure Tenant ID is empty!");
            status.escalateStatusCode(LOGACCESS_STATUS_fail);
        }

        if (m_aadClientID.length() == 0)
        {
            status.appendMessage("Target Azure Log Analytics Client Application ID is empty!");
            status.escalateStatusCode(LOGACCESS_STATUS_fail);
        }

        if (m_aadClientSecret.length()==0)
        {
            status.appendMessage("Target Azure Log Analytics Client Secret is empty!");
            status.escalateStatusCode(LOGACCESS_STATUS_fail);
        }

        if (!m_disableComponentNameJoins)
        {
            status.appendMessage("Costly query joins used to fetch component names are enabled!");
            status.escalateStatusCode(LOGACCESS_STATUS_warning);
        }

        if (!m_blobMode)
        {
            status.appendMessage("Blob mode not enabled, slow server response is likely");
            status.escalateStatusCode(LOGACCESS_STATUS_warning);
        }

        if (!targetIsContainerLogV2)
        {
            status.appendMessage("Azure Log Analytics container schema V1 enabled, V2 is recommended");
            status.escalateStatusCode(LOGACCESS_STATUS_warning);
        }

        {
            StringBuffer debugReport;
            debugReport.set("{");
            debugReport.append("\"ConnectionInfo\": {");
            appendJSONStringValue(debugReport, "TargetALAWorkspaceID", m_logAnalyticsWorkspaceID.str(), true);
            appendJSONStringValue(debugReport, "TargetALATenantID", m_aadTenantID.str(), true);
            appendJSONStringValue(debugReport, "TargetALAClientID", m_aadClientID.str(), true);
            debugReport.appendf(", \"TargetALASecret\": \"");
            if (m_aadClientSecret.isEmpty())
                debugReport.append("<EMPTY>");
            else
                maskSecret(debugReport, m_aadClientSecret, 80, false);

            debugReport.append("\"");

            appendJSONValue(debugReport, "TargetsContainerLogV2", targetIsContainerLogV2 ? true : false);
            appendJSONValue(debugReport, "ComponentsJoinedQueryEnabled", m_disableComponentNameJoins ? false : true);
            appendJSONValue(debugReport, "BlobModeEnabled", m_blobMode ? true : false);
            debugReport.append( "}"); //close conninfo

            debugReport.appendf(", \"LogMaps\": {");
            debugReport.appendf("\"Global\": { ");
            appendJSONStringValue(debugReport, "ColName", m_globalSearchColName.str(), true);
            appendJSONStringValue(debugReport, "Source", m_globalIndexSearchPattern.str(), true);
            appendJSONStringValue(debugReport, "TimeStampCol", m_globalIndexTimestampField.str(), true);
            debugReport.append(" }"); // end Global
            debugReport.appendf(", \"Components\": { ");
            appendJSONStringValue(debugReport, "ColName", m_componentsSearchColName.str(), true);
            appendJSONStringValue(debugReport, "Source", m_componentsIndexSearchPattern.str(), true);
            appendJSONStringValue(debugReport, "LookupKey", m_componentsLookupKeyColumn.str(), true);
            appendJSONStringValue(debugReport, "TimeStampCol", m_globalIndexTimestampField.str(), true);
            debugReport.appendf(" }"); // end Components
            debugReport.appendf(", \"Workunits\": { ");
            appendJSONStringValue(debugReport, "ColName", m_workunitSearchColName.str(), true);
            appendJSONStringValue(debugReport, "Source", m_workunitIndexSearchPattern.str(), true);
            appendJSONStringValue(debugReport, "TimeStampCol", m_globalIndexTimestampField.str(), true);
            debugReport.append(" }"); // end Workunits
            debugReport.appendf(", \"Audience\": { ");
            appendJSONStringValue(debugReport, "ColName", m_audienceSearchColName.str(), true);
            appendJSONStringValue(debugReport, "Source", m_audienceIndexSearchPattern.str(), true);
            debugReport.appendf(" }"); // end Audience
            debugReport.appendf(", \"Class\": { ");
            appendJSONStringValue(debugReport, "ColName", m_classSearchColName.str(), true);
            appendJSONStringValue(debugReport, "Source", m_classIndexSearchPattern.str(), true);
            debugReport.appendf(" }"); // end Class
            debugReport.appendf(", \"Instance\": { ");
            appendJSONStringValue(debugReport, "ColName", m_instanceSearchColName.str(), true);
            appendJSONStringValue(debugReport, "Source", m_instanceIndexSearchPattern.str(), true);
            appendJSONStringValue(debugReport, "LookupKey", m_instanceLookupKeyColumn.str(), true);
            debugReport.appendf(" }"); // end Instance
            debugReport.appendf(", \"Pod\": { ");
            appendJSONStringValue(debugReport, "ColName", m_podSearchColName.str(), true);
            appendJSONStringValue(debugReport, "Source", m_podIndexSearchPattern.str(), true);
            debugReport.appendf(" }"); // end Pod
            debugReport.appendf(", \"TraceID\": { ");
            appendJSONStringValue(debugReport, "ColName", m_traceSearchColName.str(), true);
            appendJSONStringValue(debugReport, "Source", m_traceIndexSearchPattern.str(), true);
            debugReport.appendf(" }"); // end TraceID
            debugReport.appendf(", \"SpanID\": { ");
            appendJSONStringValue(debugReport, "ColName", m_spanSearchColName.str(), true);
            appendJSONStringValue(debugReport, "Source", m_spanIndexSearchPattern.str(), true);
            debugReport.appendf(" }"); // end SpanID
            debugReport.appendf(", \"Host\": { ");
            appendJSONStringValue(debugReport, "ColName", m_hostSearchColName.str(), true);
            appendJSONStringValue(debugReport, "Source", m_hostIndexSearchPattern.str(), true);
            debugReport.appendf(" }"); // end Host
            debugReport.append(" }"); //close logmaps

            debugReport.append(" }"); //close debugreport

            if (options.IncludeDebugReport)
            {
                report.DebugReport.PluginDebugReport.set(debugReport);
                report.DebugReport.ServerDebugReport.set("{}");
            }
        }

        {
            StringBuffer sampleQueryReport;
            sampleQueryReport.append("{ \"TokenRequest\": { ");
            try
            {
                StringBuffer token;
                requestLogAnalyticsAccessToken(token, m_aadClientID, m_aadClientSecret, m_aadTenantID); //throws if issues encountered

                appendJSONStringValue(sampleQueryReport, "Result", token.isEmpty() ? "Error - Empty token received" : "Success", true);
            }
            catch(IException * e)
            {
                VStringBuffer description("Exception while requesting sample token (%d) - ", e->errorCode());
                e->errorMessage(description);
                status.escalateStatusCode(LOGACCESS_STATUS_fail);
                status.appendMessage(description.str());
                e->Release();
            }
            catch(...)
            {
                status.appendMessage("Unknown exception while requesting sample token");
                status.escalateStatusCode(LOGACCESS_STATUS_fail);
            }
            sampleQueryReport.append(" }"); //close sample token request

            sampleQueryReport.append(", \"Query\": { ");
            try
            {
                appendJSONStringValue(sampleQueryReport, "LogFormat", "JSON", false);
                LogAccessLogFormat outputFormat = LOGACCESS_LOGFORMAT_json;
                LogAccessConditions queryOptions;

                sampleQueryReport.appendf(", \"Filter\": { ");
                appendJSONStringValue(sampleQueryReport, "type", "byWildcard", false);
                appendJSONStringValue(sampleQueryReport, "value", "*", true);

                struct LogAccessTimeRange range;
                CDateTime endtt;
                endtt.setNow();
                range.setEnd(endtt);
                StringBuffer endstr;
                endtt.getString(endstr);

                CDateTime startt;
                startt.setNow();
                startt.adjustTimeSecs(-60); //an hour ago
                range.setStart(startt);

                StringBuffer startstr;
                startt.getString(startstr);
                sampleQueryReport.append(", \"TimeRange\": { ");
                appendJSONStringValue(sampleQueryReport, "Start", startstr.str(), false);
                appendJSONStringValue(sampleQueryReport, "End", endstr.str(), false);
                sampleQueryReport.append(" }, "); //end TimeRange

                queryOptions.setTimeRange(range);
                queryOptions.setLimit(5);
                appendJSONStringValue(sampleQueryReport, "Limit", "5", false);
                sampleQueryReport.append(" },"); //close filter
                StringBuffer queryString, queryIndex;
                populateKQLQueryString(queryString, queryIndex, queryOptions);

                appendJSONStringValue(sampleQueryReport, "KQLQuery", queryString.str(), true);
                appendJSONStringValue(sampleQueryReport, "QueryIndex", queryIndex.str(), true);

                StringBuffer logs;
                LogQueryResultDetails  resultDetails;
                fetchLog(resultDetails, queryOptions, logs, outputFormat);
                appendJSONValue(sampleQueryReport, "ResultCount", resultDetails.totalReceived);
                if (resultDetails.totalReceived==0)
                {
                    status.escalateStatusCode(LOGACCESS_STATUS_warning);
                    status.appendMessage("Query succeeded but returned 0 log entries");
                }
                sampleQueryReport.appendf(", \"Results\": %s", resultDetails.totalReceived==0 ? "\"Sample query returned zero log records!\"" : logs.str());
            }
            catch(IException * e)
            {
                VStringBuffer description("Exception while executing sample ALA query (%d) - ", e->errorCode());
                e->errorMessage(description);
                appendJSONStringValue(sampleQueryReport, "Results", description.str(), true);
                status.escalateStatusCode(LOGACCESS_STATUS_fail);
                status.appendMessage(description.str());
                e->Release();
            }
            catch(...)
            {
                appendJSONStringValue(sampleQueryReport, "Results", "Unknown exception while executing sample ALA query", false);
                status.appendMessage("Unknown exception while executing sample ALA query");
                status.escalateStatusCode(LOGACCESS_STATUS_fail);
            }
            sampleQueryReport.append(" }}"); //close sample query object and top level json container

            if (options.IncludeSampleQuery)
                report.DebugReport.SampleQueryReport.set(sampleQueryReport);
        }
    }
    catch(...)
    {
        status.escalateStatusCode(LOGACCESS_STATUS_fail);
        status.appendMessage("Encountered unexpected exception during health report");
    }

    report.status = std::move(status);
}

bool AzureLogAnalyticsCurlClient::fetchLog(LogQueryResultDetails & resultDetails, const LogAccessConditions & options, StringBuffer & returnbuf, LogAccessLogFormat format)
{
    StringBuffer token;
    requestLogAnalyticsAccessToken(token, m_aadClientID, m_aadClientSecret, m_aadTenantID); //throws if issues encountered

    if (token.isEmpty())
        throw makeStringExceptionV(-1, "%s Could not fetch valid Azure Log Analytics access token!", COMPONENT_NAME);

    StringBuffer queryString, queryIndex;
    populateKQLQueryString(queryString, queryIndex, options);

    std::string readBuffer;
    StringBuffer queryTimeSpan;
    const LogAccessTimeRange & trange = options.getTimeRange();
    azureLogAnalyticsQueryTimeSpanString(queryTimeSpan, trange.getStartt().getSimple(), trange.getEndt().isNull() ? -1 : trange.getEndt().getSimple());
    submitKQLQuery(readBuffer, token.str(), queryString.str(), m_logAnalyticsWorkspaceID.str(), queryTimeSpan.str());

    return processSearchJsonResp(resultDetails, readBuffer, returnbuf, format, true);
}

class AzureLogAnalyticsStream : public CInterfaceOf<IRemoteLogAccessStream>
{
public:
    virtual bool readLogEntries(StringBuffer & record, unsigned & recsRead) override
    {
        LogQueryResultDetails  resultDetails;
        m_remoteLogAccessor->fetchLog(resultDetails, m_options, record, m_outputFormat);
        recsRead = resultDetails.totalReceived;

        return false;
    }

    AzureLogAnalyticsStream(IRemoteLogAccess * azureQueryClient, const LogAccessConditions & options, LogAccessLogFormat format, unsigned int pageSize)
    {
        m_remoteLogAccessor.set(azureQueryClient);
        m_outputFormat = format;
        m_pageSize = pageSize;
        m_options = options;
    }

private:
    unsigned int m_pageSize;
    bool m_hasBeenScrolled = false;
    LogAccessLogFormat m_outputFormat;
    LogAccessConditions m_options;
    Owned<IRemoteLogAccess> m_remoteLogAccessor;
};

IRemoteLogAccessStream * AzureLogAnalyticsCurlClient::getLogReader(const LogAccessConditions & options, LogAccessLogFormat format)
{
    return getLogReader(options, format, defaultMaxRecordsPerFetch);
}

IRemoteLogAccessStream * AzureLogAnalyticsCurlClient::getLogReader(const LogAccessConditions & options, LogAccessLogFormat format, unsigned int pageSize)
{
    return new AzureLogAnalyticsStream(this, options, format, pageSize);
}

extern "C" IRemoteLogAccess * createInstance(IPropertyTree & logAccessPluginConfig)
{
    return new AzureLogAnalyticsCurlClient(logAccessPluginConfig);
}
