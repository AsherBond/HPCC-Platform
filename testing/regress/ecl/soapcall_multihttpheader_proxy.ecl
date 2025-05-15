/*##############################################################################

    HPCC SYSTEMS software Copyright (C) 2023 HPCC Systems®.

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

//class=proxy
//class=3rdparty

string TargetIP := '.' : stored('TargetIP');
string storedHeader := 'StoredHeaderDefault' : stored('storedHeader');

httpEchoServiceResponseRecord :=
    RECORD
        string method{xpath('Method')};
        string path{xpath('UrlPath')};
        string parameters{xpath('UrlParameters')};
        set of string headers{xpath('Headers/Header')};
        string content{xpath('Content')};
    END;

string TargetURL := 'http://' + TargetIP + ':8010/WsSmc/HttpEcho?name=doe,joe&number=1';


httpEchoServiceRequestRecord :=
    RECORD
       string Name{xpath('Name')} := 'Doe, Joe',
       unsigned id{xpath('ADL')} := 999999,
       real8 score := 88.88,
    END;

string constHeader := 'constHeaderValue';

//test proxyaddress functionality by using tinyproxy on port 8888
string targetProxy := 'http://' + 'localhost' + ':8888';

proxyResult := SOAPCALL(TargetURL, 'HttpEcho', httpEchoServiceRequestRecord, DATASET(httpEchoServiceResponseRecord), LITERAL, xpath('HttpEchoResponse'), proxyAddress(TargetProxy),
                TIMEOUT(30), TIMELIMIT(40),
                httpheader('StoredHeader', storedHeader), httpheader('literalHeader', 'literalHeaderValue'), httpheader('constHeader', constHeader),
                httpheader('HPCC-Global-Id','9876543210'), httpheader('HPCC-Caller-Id','http222'),
                httpheader('traceparent', '00-0123456789abcdef0123456789abcdef-f123456789abcdef-01'));

output(proxyResult, named('proxyResult'));
