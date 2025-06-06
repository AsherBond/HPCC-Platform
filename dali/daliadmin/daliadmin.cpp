/*##############################################################################

    HPCC SYSTEMS software Copyright (C) 2012 HPCC Systems®.

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

#include "daclient.hpp"
#include "dadfs.hpp"
#include "rmtfile.hpp"
#include "daadmin.hpp"

#include "ws_dfsclient.hpp"


using namespace daadmin;

#define DEFAULT_DALICONNECT_TIMEOUT 5 // seconds

void doLog(bool noError, StringBuffer &out)
{
    if (noError)
        PROGLOG("%s", out.str());
    else
        UERRLOG("%s", out.str());
}

void usage(const char *exe)
{
  printf("Usage:\n");
  printf("  %s [<daliserver-ip>] <command> { <option> }\n", exe);              
  printf("\n");
  printf("Data store commands:\n");
  printf("  export <branchxpath> <destfile>\n");
  printf("  import <branchxpath> <srcfile>\n");
  printf("  importadd <branchxpath> <srcfile>\n");
  printf("  delete <branchxpath> [nobackup] -- delete branch, 'nobackup' option suppresses writing copy of existing branch\n");
  printf("  set <xpath> <value>        -- set single value\n");
  printf("  get <xpath>                -- get single value\n");
  printf("  bget <xpath> <dest-file>   -- binary property\n");
  printf("  xget <xpath>               -- (multi-value tail can have commas)\n");
  printf("  wget <xpath>               -- (gets all matching xpath)\n");
  printf("  add <xpath> [<value>]      -- adds new xpath node with optional value\n");
  printf("  delv <xpath>               -- deletes value\n");
  printf("  count <xpath>              -- counts xpath matches\n");
  printf("\n");
  printf("Logical File meta information commands:\n");
  printf("  checksuperfile <superfilename> [fix=true|false] -- check superfile links consistent and optionally fix\n");
  printf("  checksubfile <subfilename>     -- check subfile links to parent consistent\n");
  printf("  checkfilesize <logicalfilemask> [fix=true|false] -- check file size attributes and optionally fix");
  printf("  cleanscopes                    -- remove empty scopes\n");
  printf("  clusternodes <clustername> [filename] -- get IPs for cluster group. Written to optional filename if provided\n");
  printf("  dfscheck                       -- verify dfs file information is valid\n");
  printf("  dfscompratio <logicalname>     -- returns compression ratio of file\n");
  printf("  dfscsv <logicalnamemask>       -- get csv info. for files matching mask\n");
  printf("  dfsexists <logicalname>        -- sets return value to 0 if file exists\n");
  printf("  dfsfile <logicalname>          -- get meta information for file\n");
  printf("  dfsgroup <logicalgroupname> [filename] -- get IPs for logical group (aka cluster). Written to optional filename if provided\n");
  printf("  dfsls [<logicalname>] [options]-- get list of files within a scope (options=lrs)\n");
  printf("  dfsmap <logicalname>           -- get part files (primary and replicates)\n");
  printf("  dfsmeta <logicalname> <storage> -- get new meta information for file\n");
  printf("  dfsparents <logicalname>       -- list superfiles containing file\n");
  printf("  dfspart <logicalname> <part>   -- get meta information for part num\n");
  printf("  dfsperm <logicalname>          -- returns LDAP permission for file\n");
  printf("  dfsreplication <clustermask> <logicalnamemask> <redundancy-count> [dryrun] -- set redundancy for files matching mask, on specified clusters only\n");
  printf("  dfsscopes <mask>               -- lists logical scopes (mask = * for all)\n");
  printf("  dfsunlink <logicalname>        -- unlinks file from all super parents\n");
  printf("  dfsverify <logicalname>        -- verifies parts exist, returns 0 if ok\n");
  printf("  holdlock <logicalfile> <read|write> -- hold a lock to the logical-file until a key is pressed");
  printf("  listexpires <logicalnamemask>  -- lists logical files with expiry value\n");
  printf("  listprotect <logicalnamemask>  <id-mask> -- list protected files\n");
  printf("  listrelationships <primary> <secondary>\n");
  printf("  normalizefilenames [<logicalnamemask>] -- normalize existing logical filenames that match, e.g. .::.::scope::.::name -> scope::name\n");
  printf("  setdfspartattr <logicalname> <part> <attribute> [<value>] -- set attribute of a file part to value, or delete the attribute if not provided\n");
  printf("  setprotect <logicalname> <id>  -- overwrite protects logical file\n");
  printf("  unprotect <logicalname> <id>   -- unprotect (if id=* then clear all)\n");
  printf("\n");
  printf("Workunit commands:\n");
  printf("  listworkunits [<prop>=<val> [<lower> [<upper>]]] -- list workunits that match prop=val in workunit name range lower to upper\n");
  printf("  listmatches <connection xpath> [<match xpath>=<val> [<property xpaths>]] -- <property xpaths> is comma separated list of xpaths\n");
  printf("  workunittimings <WUID>\n");
  printf("\n");
  printf("Other dali server and misc commands:\n");
  printf("  auditlog <fromdate> <todate> <match>\n");
  printf("  cleanglobalwuid [dryrun] [noreconstruct]\n");
  printf("  cleanjobqueues [dryrun]\n");
  printf("  cleangenerateddlls [dryrun] [nobackup]\n");
  printf("  cleanstalegroups [<group-pattern>] [dryrun]\n");
  printf("  clusterlist <mask>              -- list clusters   (mask optional)\n");
  printf("  coalesce                        -- force transaction coalesce\n");
  printf("  dalilocks [ <ip-pattern> ] [ files ] -- get all locked files/xpaths\n");
  printf("  daliping [ <num> ]              -- time dali server connect\n");
  printf("  getxref <destxmlfile>           -- get all XREF information\n");
  printf("  loadxml <srcxmlfile> [--lowmem[=<true|false]]    -- use lowmem AtomPTree's\n"
         "                       [--parseonly[=<true|false]] -- parse the xml file, don't load it into dali\n"
         "                       [--savexml[=<true|false]]   -- save and time the parsed xml\n");
  printf("  migratefiles <src-group> <target-group> [<filemask>] [dryrun] [createmaps] [listonly] [verbose]\n");
  printf("  mpping <server-ip>              -- time MP connect\n");
  printf("  serverlist <mask>               -- list server IPs (mask optional)\n");
  printf("  translatetoxpath logicalfile [File|SuperFile|Scope]\n");
  printf("  unlock <xpath or logicalfile> <[path|file]> --  unlocks either matching xpath(s) or matching logical file(s), can contain wildcards\n");
  printf("  validatestore [fix=<true|false>]\n"
         "                [verbose=<true|false>]\n"
         "                [deletefiles=<true|false>]-- perform some checks on dali meta data an optionally fix or remove redundant info \n");
  printf("  workunit <workunit> [true]      -- dump workunit xml, if 2nd parameter equals true, will also include progress data\n");
  printf("  wuidcompress <wildcard> <type>  --  scan workunits that match <wildcard> and compress resources of <type>\n");
  printf("  wuiddecompress <wildcard> <type> --  scan workunits that match <wildcard> and decompress resources of <type>\n");
  printf("  xmlsize <filename> [<percentage>] --  analyse size usage in xml file, display individual items above 'percentage' \n");
  printf("\n");
  printf("Common options\n");
  printf("  server=<dali-server-ip>         -- server ip\n");
  printf("                                  -- can be 1st param if numeric ip (or '.')\n");
  printf("  user=<username>                 -- for file operations\n");
  printf("  password=<password>             -- for file operations\n");
  printf("  logfile=<filename>              -- filename blank for no log\n");
  printf("  rawlog=0|1                      -- if raw omits timestamps etc\n");
  printf("  timeout=<seconds>               -- set dali connect timeout\n");
}

#define CHECKPARAMS(mn,mx) { if ((np<mn)||(np>mx)) throw MakeStringException(-1,"%s: incorrect number of parameters",cmd); }

static constexpr const char * defaultYaml = R"!!(
version: "1.0"
daliadmin:
  name: daliadmin
)!!";

static void remoteTest(const char *logicalName, bool withDali);

int main(int argc, const char* argv[])
{
    int ret = 0;
    InitModuleObjects();
    EnableSEHtoExceptionMapping();
    setDaliServixSocketCaching(true);
    if (argc<2) {
        usage(argv[0]);
        return -1;
    }

    Owned<IPropertyTree> globals = loadConfiguration(defaultYaml, argv, "daliadmin", "DALIADMIN", "daliadmin.xml", nullptr, nullptr, false);
    Owned<IProperties> props = createProperties("daliadmin.ini");
    StringArray params;
    SocketEndpoint ep;
    StringBuffer tmps;
    for (int i=1;i<argc;i++) {
        const char *param = argv[i];
        if (startsWith(param, "--"))
            continue; // handled by loadConfiguration
        if ((memcmp(param,"server=",7)==0)||
            (memcmp(param,"logfile=",8)==0)||
            (memcmp(param,"rawlog=",7)==0)||
            (memcmp(param,"user=",5)==0)||
            (memcmp(param,"password=",9)==0) ||
            (memcmp(param,"fix=",4)==0) ||
            (memcmp(param,"verbose=",8)==0) ||
            (memcmp(param,"deletefiles=",12)==0) ||
            (memcmp(param,"timeout=",8)==0))
            props->loadProp(param);
        else if ((i==1)&&(isdigit(*param)||(*param=='.'))&&ep.set(((*param=='.')&&param[1])?(param+1):param,DALI_SERVER_PORT))
            props->setProp("server",ep.getEndpointHostText(tmps.clear()).str());
        else {
            if ((strieq(param,"help")) || (strieq(param,"-help")) || (strieq(param,"--help"))) {
                usage(argv[0]);
                return -1;
            }
            params.append(param);
        }
    }
    if (!params.ordinality()) {
        usage(argv[0]);
        return -1;
    }

    try {
        StringBuffer logname;
        StringBuffer aliasname;
        bool rawlog = props->getPropBool("rawlog");
        Owned<ILogMsgHandler> fileMsgHandler;
        if (props->getProp("logfile",logname)) {
            if (logname.length()) {
                fileMsgHandler.setown(getFileLogMsgHandler(logname.str(), NULL, rawlog?MSGFIELD_prefix:MSGFIELD_STANDARD, LOGFORMAT_table, false, true));
                queryLogMsgManager()->addMonitorOwn(fileMsgHandler.getClear(), getCategoryLogMsgFilter(MSGAUD_all, MSGCLS_all, TopDetail));
            }
        }
        // set stdout 
        attachStandardHandleLogMsgMonitor(stdout,0,MSGAUD_all,MSGCLS_all&~(MSGCLS_disaster|MSGCLS_error|MSGCLS_warning));
        Owned<ILogMsgFilter> filter = getCategoryLogMsgFilter(MSGAUD_user, MSGCLS_error|MSGCLS_warning);
        queryLogMsgManager()->changeMonitorFilter(queryStderrLogMsgHandler(), filter);
        queryStderrLogMsgHandler()->setMessageFields(MSGFIELD_prefix);
    }
    catch (IException *e) {
        pexception("daliadmin",e);
        e->Release();
        ret = 255;
    }
    unsigned daliconnectelapsed;
    StringBuffer daliserv;
    if (!ret) {
        const char *cmd = params.item(0);
        unsigned np = params.ordinality()-1;

        if (!props->getProp("server",daliserv.clear()))
        {
            // external commands
            try
            {
                if (strieq(cmd,"xmlsize"))
                {
                    CHECKPARAMS(1,2);
                    xmlSize(params.item(1), np>1?atof(params.item(2)):1.0);
                }
                else if (strieq(cmd,"translatetoxpath"))
                {
                    CHECKPARAMS(1,2);
                    DfsXmlBranchKind branchType;
                    if (np>1)
                    {
                        const char *typeStr = params.item(2);
                        branchType = queryDfsXmlBranchType(typeStr);
                    }
                    else
                        branchType = DXB_File;
                    translateToXpath(params.item(1), branchType);
                }
                else if (strieq(cmd, "remotetest"))
                    remoteTest(params.item(1), false);
                else if (strieq(cmd, "loadxml"))
                {
                    bool useLowMemPTree = false;
                    bool saveFormatedTree = false;
                    bool freePTree = false;
                    bool parseOnly = getComponentConfigSP()->getPropBool("@parseonly");
                    if (!parseOnly)
                    {
                        useLowMemPTree = getComponentConfigSP()->getPropBool("@lowmem");
                        saveFormatedTree = getComponentConfigSP()->getPropBool("@savexml");
                        freePTree = getComponentConfigSP()->getPropBool("@free");
                    }
                    loadXMLTest(params.item(1), parseOnly, useLowMemPTree, saveFormatedTree, freePTree);
                }
                else
                {
                    UERRLOG("Unknown command %s",cmd);
                    ret = 255;
                }
            }
            catch (IException *e)
            {
                EXCLOG(e,"daliadmin");
                e->Release();
                ret = 255;
            }
            return ret;
        }
        else
        {
            try {
                SocketEndpoint ep(daliserv.str(),DALI_SERVER_PORT);
                SocketEndpointArray epa;
                epa.append(ep);
                Owned<IGroup> group = createIGroup(epa);
                unsigned start = msTick();
                initClientProcess(group, DCR_DaliAdmin);
                daliconnectelapsed = msTick()-start;
            }
            catch (IException *e) {
                EXCLOG(e,"daliadmin initClientProcess");
                e->Release();
                ret = 254;
            }
            if (!ret) {
                try {
                    Owned<IUserDescriptor> userDesc;
                    if (props->getProp("user",tmps.clear())) {
                        userDesc.setown(createUserDescriptor());
                        StringBuffer ps;
                        props->getProp("password",ps);
                        userDesc->set(tmps.str(),ps.str());
                        queryDistributedFileDirectory().setDefaultUser(userDesc);
                    }
                    StringBuffer out;
                    setDaliConnectTimeoutMs(1000 * props->getPropInt("timeout", DEFAULT_DALICONNECT_TIMEOUT));
                    if (strieq(cmd,"export")) {
                        CHECKPARAMS(2,2);
                        exportToFile(params.item(1),params.item(2));
                    }
                    else if (strieq(cmd,"import")) {
                        CHECKPARAMS(2,2);
                        doLog(importFromFile(params.item(1),params.item(2),false,out),out);
                    }
                    else if (strieq(cmd,"importadd")) {
                        CHECKPARAMS(2,2);
                        doLog(importFromFile(params.item(1),params.item(2),true,out),out);
                    }
                    else if (strieq(cmd,"delete")) {
                        CHECKPARAMS(1,2);
                        bool backup = np<2 || !strieq("nobackup", params.item(2));
                        doLog(erase(params.item(1),backup,out),out);
                    }
                    else if (strieq(cmd,"set")) {
                        CHECKPARAMS(2,2);
                        setValue(params.item(1),params.item(2),out);
                        PROGLOG("Changed %s from '%s' to '%s'",params.item(1),out.str(),params.item(2));
                    }
                    else if (strieq(cmd,"get")) {
                        CHECKPARAMS(1,1);
                        getValue(params.item(1),out);
                        PROGLOG("Value of %s is: '%s'",params.item(1),out.str());
                    }
                    else if (strieq(cmd,"bget")) {
                        CHECKPARAMS(2,2);
                        bget(params.item(1),params.item(2));
                    }
                    else if (strieq(cmd,"wget")) {
                        CHECKPARAMS(1,1);
                        wget(params.item(1));
                    }
                    else if (strieq(cmd,"xget")) {
                        CHECKPARAMS(1,1);
                        wget(params.item(1));
                    }
                    else if (strieq(cmd,"add")) {
                        CHECKPARAMS(1,2);
                        doLog(add(params.item(1),(np>1) ? params.item(2) : nullptr,out),out);
                    }
                    else if (strieq(cmd,"delv")) {
                        CHECKPARAMS(1,1);
                        delv(params.item(1));
                    }
                    else if (strieq(cmd,"count")) {
                        CHECKPARAMS(1,1);
                        PROGLOG("Count of %s is: %d",params.item(1),count(params.item(1)));
                    }
                    else if (strieq(cmd,"dfsfile")) {
                        CHECKPARAMS(1,1);
                        doLog(dfsfile(params.item(1),userDesc,out),out);
                    }
                    else if (strieq(cmd,"dfsmeta")) {
                        CHECKPARAMS(1,3);
                        bool includeStorage = (np < 2) || strToBool(params.item(2));
                        dfsmeta(params.item(1),userDesc,includeStorage);
                    }
                    else if (strieq(cmd,"dfspart")) {
                        CHECKPARAMS(2,2);
                        doLog(dfspart(params.item(1),userDesc,atoi(params.item(2)),out),out);                        
                    }
                    else if (strieq(cmd,"setdfspartattr")) {
                        CHECKPARAMS(3,4);
                        setdfspartattr(params.item(1),atoi(params.item(2)),params.item(3),np>3?params.item(4):nullptr,userDesc,out);
                        PROGLOG("%s", out.str());
                    }
                    else if (strieq(cmd,"dfscheck")) {
                        CHECKPARAMS(0,0);
                        doLog(dfsCheck(out),out);
                    }
                    else if (strieq(cmd,"dfscsv")) {
                        CHECKPARAMS(1,1);
                        dfscsv(params.item(1),userDesc,out);
                        PROGLOG("%s",out.str());
                    }
                    else if (strieq(cmd,"dfsgroup")) {
                        CHECKPARAMS(1,2);
                        dfsGroup(params.item(1),(np>1)?params.item(2):NULL);
                    }
                    else if (strieq(cmd,"clusternodes")) {
                        CHECKPARAMS(1,2);
                        ret = clusterGroup(params.item(1),(np>1)?params.item(2):NULL);
                    }
                    else if (strieq(cmd,"dfsls")) {
                        CHECKPARAMS(0,2);
                        doLog(dfsLs((np>0)?params.item(1):nullptr,(np>1)?params.item(2):nullptr,out),out);
                    }
                    else if (strieq(cmd,"dfsmap")) {
                        CHECKPARAMS(1,1);
                        doLog(dfsmap(params.item(1),userDesc,out),out);
                    }
                    else if (strieq(cmd,"dfsexists") || strieq(cmd,"dfsexist")) {
                        // NB: "dfsexist" typo', kept for backward compatibility only (<7.12)
                        CHECKPARAMS(1,1);
                        ret = dfsexists(params.item(1),userDesc);
                    }
                    else if (strieq(cmd,"dfsparents")) {
                        CHECKPARAMS(1,1);
                        dfsparents(params.item(1),userDesc,out);
                        PROGLOG("%s",out.str());
                    }
                    else if (strieq(cmd,"dfsunlink")) {
                        CHECKPARAMS(1,1);
                        dfsunlink(params.item(1),userDesc);
                    }
                    else if (strieq(cmd,"dfsverify")) {
                        CHECKPARAMS(1,1);
                        ret = dfsverify(params.item(1),NULL,userDesc);
                    }
                    else if (strieq(cmd,"setprotect")) {
                        CHECKPARAMS(2,2);
                        setprotect(params.item(1),params.item(2),userDesc,out);
                        PROGLOG("%s",out.str());
                    }
                    else if (strieq(cmd,"unprotect")) {
                        CHECKPARAMS(2,2);
                        unprotect(params.item(1),params.item(2),userDesc,out);
                        PROGLOG("%s",out.str());
                    }
                    else if (strieq(cmd,"listprotect")) {
                        CHECKPARAMS(0,2);
                        listprotect((np>1)?params.item(1):"*",(np>2)?params.item(2):"*",out);
                        PROGLOG("%s",out.str());
                    }
                    else if (strieq(cmd,"checksuperfile")) {
                        CHECKPARAMS(1,1);
                        bool fix = props->getPropBool("fix");
                        checksuperfile(params.item(1),fix);
                    }
                    else if (strieq(cmd,"checksubfile")) {
                        CHECKPARAMS(1,1);
                        checksubfile(params.item(1));
                    }
                    else if (strieq(cmd,"listexpires")) {
                        CHECKPARAMS(0,1);
                        listexpires((np>1)?params.item(1):"*",userDesc);
                    }
                    else if (strieq(cmd,"listrelationships")) {
                        CHECKPARAMS(2,2);
                        listrelationships(params.item(1),params.item(2));
                    }
                    else if (strieq(cmd,"dfsperm")) {
                        if (!userDesc.get())
                            throw MakeStringException(-1,"dfsperm requires username to be set (user=)");
                        CHECKPARAMS(1,1);
                        ret = dfsperm(params.item(1),userDesc);
                    }
                    else if (strieq(cmd,"dfscompratio")) {
                        CHECKPARAMS(1,1);
                        dfscompratio(params.item(1),userDesc);
                    }
                    else if (strieq(cmd,"dfsscopes")) {
                        CHECKPARAMS(0,1);
                        dfsscopes((np>0)?params.item(1):"*",userDesc);
                    }
                    else if (strieq(cmd,"cleanscopes")) {
                        CHECKPARAMS(0,0);
                        cleanscopes(userDesc);
                    }
                    else if (strieq(cmd,"normalizefilenames")) {
                        CHECKPARAMS(0,1);
                        normalizeFileNames(userDesc, np>0 ? params.item(1) : nullptr);
                    }
                    else if (strieq(cmd,"listworkunits")) {
                        CHECKPARAMS(0,3);
                        listworkunits((np>0)?params.item(1):NULL,(np>1)?params.item(2):NULL,(np>2)?params.item(3):NULL);
                    }
                    else if (strieq(cmd,"listmatches")) {
                        CHECKPARAMS(0,3);
                        listmatches((np>0)?params.item(1):NULL,(np>1)?params.item(2):NULL,(np>2)?params.item(3):NULL);
                    }
                    else if (strieq(cmd,"workunittimings")) {
                        CHECKPARAMS(1,1);
                        workunittimings(params.item(1));
                    }
                    else if (strieq(cmd,"serverlist")) {
                        CHECKPARAMS(1,1);
                        serverlist(params.item(1));
                    }
                    else if (strieq(cmd,"clusterlist")) {
                        CHECKPARAMS(1,1);
                        clusterlist(params.item(1));
                    }
                    else if (strieq(cmd,"auditlog")) {
                        CHECKPARAMS(2,3);
                        auditlog(params.item(1),params.item(2),(np>2)?params.item(3):NULL);
                    }
                    else if (strieq(cmd,"coalesce")) {
                        CHECKPARAMS(0,0);
                        coalesce();
                    }
                    else if (strieq(cmd,"mpping")) {
                        CHECKPARAMS(1,1);
                        mpping(params.item(1));
                    }
                    else if (strieq(cmd,"daliping")) {
                        CHECKPARAMS(0,1);
                        daliping(daliserv.str(),daliconnectelapsed,(np>0)?atoi(params.item(1)):1);
                    }
                    else if (strieq(cmd,"getxref")) {
                        CHECKPARAMS(1,1);
                        getxref(params.item(1));
                    }
                    else if (strieq(cmd,"checkfilesize")) {
                        CHECKPARAMS(1,1);
                        bool fix = props->getPropBool("fix");
                        checkFileSize(userDesc, params.item(1), fix);
                    }
                    else if (strieq(cmd,"dalilocks")) {
                        CHECKPARAMS(0,2);
                        bool filesonly = false;
                        if (np&&(strieq(params.item(np),"files"))) {
                            filesonly = true;
                            np--;
                        }
                        dalilocks(np>0?params.item(1):NULL,filesonly);
                    }
                    else if (strieq(cmd,"unlock")) {
                        CHECKPARAMS(2,2);
                        const char *fileOrPath = params.item(2);
                        if (strieq("file", fileOrPath))
                            unlock(params.item(1), true);
                        else if (strieq("path", fileOrPath))
                            unlock(params.item(1), false);
                        else
                            throw MakeStringException(0, "unknown type [ %s ], must be 'file' or 'path'", fileOrPath);
                    }
                    else if (strieq(cmd,"validateStore")) {
                        CHECKPARAMS(0,2);
                        bool fix = props->getPropBool("fix");
                        bool verbose = props->getPropBool("verbose");
                        bool deleteFiles = props->getPropBool("deletefiles");
                        validateStore(fix, deleteFiles, verbose);
                    }
                    else if (strieq(cmd, "workunit")) {
                        CHECKPARAMS(1,2);
                        bool includeProgress=false;
                        if (np>1)
                            includeProgress = strToBool(params.item(2));
                        dumpWorkunit(params.item(1), includeProgress);
                    }
                    else if (strieq(cmd,"wuidCompress")) {
                        CHECKPARAMS(2,2);
                        wuidCompress(params.item(1), params.item(2), true);
                    }
                    else if (strieq(cmd,"wuidDecompress")) {
                        CHECKPARAMS(2,2);
                        wuidCompress(params.item(1), params.item(2), false);
                    }
                    else if (strieq(cmd,"dfsreplication")) {
                        CHECKPARAMS(3,4);
                        bool dryRun = np>3 && strieq("dryrun", params.item(4));
                        dfsreplication(params.item(1), params.item(2), atoi(params.item(3)), dryRun);
                    }
                    else if (strieq(cmd,"holdlock")) {
                        CHECKPARAMS(2,2);
                        holdlock(params.item(1), params.item(2), userDesc);
                    }
                    else if (strieq(cmd, "progress")) {
                        CHECKPARAMS(2,2);
                        dumpProgress(params.item(1), params.item(2));
                    }
                    else if (strieq(cmd, "migratefiles"))
                    {
                        CHECKPARAMS(2, 7);
                        const char *srcGroup = params.item(1);
                        const char *dstGroup = params.item(2);
                        const char *filemask = "*";
                        StringBuffer options;
                        if (params.isItem(3))
                        {
                            filemask = params.item(3);
                            unsigned arg=4;
                            StringArray optArray;
                            while (arg<params.ordinality())
                                optArray.append(params.item(arg++));
                            optArray.getString(options, ",");
                        }
                        migrateFiles(srcGroup, dstGroup, filemask, options);
                    }
                    else if (stricmp(cmd, "wuattr") == 0) {
                        CHECKPARAMS(1, 2);
                        if (params.ordinality() > 2)
                            dumpWorkunitAttr(params.item(1), params.item(2));
                        else
                            dumpWorkunitAttr(params.item(1), nullptr);
                    }
                    else if (strieq(cmd, "cleanglobalwuid"))
                    {
                        CHECKPARAMS(0, 2);
                        bool dryrun = false;
                        bool reconstruct = true;
                        for (unsigned i=1; i<params.ordinality(); i++)
                        {
                            const char *param = params.item(i);
                            if (strieq("dryrun", param))
                                dryrun = true;
                            else if (strieq("noreconstruct", param))
                                reconstruct = false;
                        }
                        removeOrphanedGlobalVariables(dryrun, reconstruct);
                    }
                    else if (strieq(cmd, "cleanjobqueues"))
                    {
                        bool dryRun = np>0 && strieq("dryrun", params.item(1));
                        cleanJobQueues(dryRun);
                    }
                    else if (strieq(cmd, "cleangenerateddlls"))
                    {
                        bool dryRun = false;
                        bool backup = true; // default
                        for (unsigned i=1; i<params.ordinality(); i++)
                        {
                            const char *param = params.item(i);
                            if (strieq("dryrun", param))
                                dryRun = true;
                            else if (strieq("nobackup", param))
                                backup = false;
                        }
                        cleanGeneratedDlls(dryRun, backup);
                    }
                    else if (strieq(cmd, "cleanstalegroups"))
                    {
                        // The 'cleanstalegroups' command removes stale groups from the system.
                        // Parameters:
                        //   1. (Optional) groupPattern: A string pattern to filter groups. If not provided, all groups are considered.
                        //   2. (Optional) "dryrun": A flag indicating that no actual changes will be made. Can appear as the first or second parameter.
                        // Example usage:
                        //   cleanstalegroups "myGroup*" dryrun
                        //   cleanstalegroups dryrun
                        CHECKPARAMS(0, 2);
                        bool dryrun = false;
                        const char *groupPattern = nullptr;
                        if (np > 0)
                        {
                            const char *param = params.item(1);
                            if (strieq(param, "dryrun"))
                                dryrun = true;
                            else
                            {
                                groupPattern = param;
                                if (np > 1)
                                {
                                    const char *param = params.item(2);
                                    if (strieq(param, "dryrun"))
                                        dryrun = true;
                                }
                            }
                        }
                        cleanStaleGroups(groupPattern, dryrun);
                    }
                    else if (strieq(cmd, "remotetest"))
                        remoteTest(params.item(1), true);
                    else
                        UERRLOG("Unknown command %s",cmd);
                }
                catch (IException *e)
                {
                    EXCLOG(e,"daliadmin");
                    e->Release();
                    ret = 255;
                }
                closedownClientProcess();
            }
        }
    }
    setDaliServixSocketCaching(false);
    setNodeCaching(false);
    releaseAtoms();
    fflush(stdout);
    fflush(stderr);
    return ret;
}

static void testDFSFile(IDistributedFile *legacyDfsFile, const char *logicalName)
{
    unsigned numParts = legacyDfsFile->numParts();
    IDistributedFilePart &part0 = legacyDfsFile->queryPart(0);
    Owned<IDistributedFilePart> part0b = legacyDfsFile->getPart(0);
    StringBuffer partName;
    part0b->getPartName(partName);
    PROGLOG("partName: %s", partName.str());
    StringBuffer lfn;
    legacyDfsFile->getLogicalName(lfn);
    PROGLOG("lfn: %s", lfn.str());
    verifyex(streq(logicalName, lfn));
    const char *lfnb = legacyDfsFile->queryLogicalName();
    verifyex(streq(logicalName, lfnb));
    Owned<IDistributedFilePartIterator> iter = legacyDfsFile->getIterator();
    ForEach(*iter)
    {
        IDistributedFilePart &part = iter->query();
        part.getPartName(partName.clear());
        PROGLOG("partName: %s", partName.str());
    }
    Owned<IFileDescriptor> dfsFileDesc = legacyDfsFile->getFileDescriptor();
    const char *dir = legacyDfsFile->queryDefaultDir();
    PROGLOG("dir = %s", dir);
    const char *mask = legacyDfsFile->queryPartMask();
    PROGLOG("mask = %s", mask);
    IPropertyTree &attrs = legacyDfsFile->queryAttributes();
    legacyDfsFile->lockProperties();
    legacyDfsFile->unlockProperties();
    CDateTime dt;
    legacyDfsFile->getModificationTime(dt);
    StringBuffer dateString;
    dt.getString(dateString);
    PROGLOG("Modification time: %s", dateString.str());

    legacyDfsFile->getAccessedTime(dt);
    dt.getString(dateString.clear());
    PROGLOG("Accessed time: %s", dateString.str());
    unsigned numCopies = legacyDfsFile->numCopies(0);
    PROGLOG("numCopies: %d", numCopies);
    bool forcePhysical = false;
    if (!legacyDfsFile->existsPhysicalPartFiles(0))
        WARNLOG("Could not find physical part files");
    else
        forcePhysical = true;
    __int64 dfsSz = legacyDfsFile->getFileSize(true, forcePhysical);
    PROGLOG("dfsSz: %" I64F "d", dfsSz);
    __int64 diskSz = legacyDfsFile->getDiskSize(true, forcePhysical);
    PROGLOG("diskSz: %" I64F "d", diskSz);
    unsigned checkSum;
    legacyDfsFile->getFileCheckSum(checkSum);
    PROGLOG("checkSum: %d", checkSum);
    offset_t base;
    legacyDfsFile->getPositionPart(0, base);
    PROGLOG("base: %" I64F "d", base);
    IDistributedSuperFile *super = legacyDfsFile->querySuperFile();
    PROGLOG("isSuper: %s", boolToStr(super!=NULL));
    Owned<IDistributedSuperFileIterator> ownerIter = legacyDfsFile->getOwningSuperFiles();
    ForEach(*ownerIter)
    {
        IDistributedSuperFile &superFile = ownerIter->query();
        PROGLOG("superName: %s", superFile.queryLogicalName());
    }
    bool compressed = legacyDfsFile->isCompressed();
    PROGLOG("compressed: %s", boolToStr(compressed));
    StringBuffer clusterName;
    legacyDfsFile->getClusterName(0, clusterName);
    PROGLOG("clusterName: %s", clusterName.str());
    StringArray clusters;
    unsigned numClusterNames = legacyDfsFile->getClusterNames(clusters);
    ForEachItemIn(i, clusters)
    {
        PROGLOG("clusterName: %s", clusters.item(i));
    }
    unsigned numClusters = legacyDfsFile->numClusters();
    PROGLOG("numClusters: %d", numClusters);
    unsigned clusterNamePos = legacyDfsFile->findCluster(clusterName);
    PROGLOG("clusterNamePos: %d", clusterNamePos);
    ClusterPartDiskMapSpec &mapSpec = legacyDfsFile->queryPartDiskMapping(0);
    IGroup *group = legacyDfsFile->queryClusterGroup(0);
    StringBuffer clusterGroupName;
    legacyDfsFile->getClusterGroupName(0, clusterGroupName);
    PROGLOG("clusterGroupName: %s", clusterGroupName.str());
    StringBuffer ecl;
    legacyDfsFile->getECL(ecl);
    PROGLOG("ecl: %s", ecl.str());
    StringBuffer reason;
    bool canModify = legacyDfsFile->canModify(reason);
    PROGLOG("canModify: %s", boolToStr(canModify));
    bool canRemove = legacyDfsFile->canRemove(reason.clear());
    PROGLOG("canRemove: %s", boolToStr(canRemove));
    StringBuffer err;
    bool compat = legacyDfsFile->checkClusterCompatible(*dfsFileDesc, err);
    PROGLOG("compat: %s", boolToStr(compat));
    unsigned crc;
    legacyDfsFile->getFormatCrc(crc);
    PROGLOG("crc: %d", crc);
    size32_t rsz;
    legacyDfsFile->getRecordSize(rsz);
    PROGLOG("rsz: %d", rsz);
    MemoryBuffer layout;
    legacyDfsFile->getRecordLayout(layout, "_rtlType");
    StringBuffer mapping;
    legacyDfsFile->getColumnMapping(mapping);
    PROGLOG("mapping: %s", mapping.str());
    bool restricted = legacyDfsFile->isRestrictedAccess();
    PROGLOG("restricted: %s", boolToStr(restricted));
    unsigned oldTimeout = legacyDfsFile->setDefaultTimeout(3500);
    PROGLOG("oldTimeout: %d", oldTimeout);

    try
    {
        legacyDfsFile->validate();
    }
    catch (IException *e)
    {
        EXCLOG(e);
        e->Release();
    }

    IPropertyTree *history = legacyDfsFile->queryHistory();
    bool isExternal = legacyDfsFile->isExternal();
    PROGLOG("isExternal: %s", boolToStr(isExternal));
    unsigned maxSkew, minSkew, maxSkewPart, minSkewPart;
    if (legacyDfsFile->getSkewInfo(maxSkew, minSkew, maxSkewPart, minSkewPart, true))
    {
        PROGLOG("maxSkew: %d", maxSkew);
        PROGLOG("minSkew: %d", minSkew);
        PROGLOG("maxSkewPart: %d", maxSkewPart);
        PROGLOG("minSkewPart: %d", minSkewPart);
    }
    int expire = legacyDfsFile->getExpire(nullptr);
    PROGLOG("expire: %d", expire);
    try
    {
        cost_type atRestCost, accessCost;
        legacyDfsFile->getCost(clusterName.str(), atRestCost, accessCost);
        PROGLOG("accessCost: %f", cost_type2money(accessCost));
        PROGLOG("atRestCost: %f", cost_type2money(atRestCost));
    }
    catch(IException *e)
    {
        EXCLOG(e);
        e->Release();
    }

    // test some write methods. NB: at the moment, in common with foreign files, these changes do not get propagaged to dali
    legacyDfsFile->setModified();
    dt.adjustTime(30);
    legacyDfsFile->setAccessedTime(dt);
    legacyDfsFile->setAccessed();
    legacyDfsFile->addAttrValue("recordCount", 10);
    legacyDfsFile->setExpire(10);
    legacyDfsFile->setECL("1;");
    legacyDfsFile->resetHistory();
    legacyDfsFile->setProtect("me", true);
    legacyDfsFile->setColumnMapping("field1");
    legacyDfsFile->setRestrictedAccess(true);

    // this is simulating what happens in Thor when the manager serializes parts to workers
    Owned<IFileDescriptor> fileDesc = legacyDfsFile->getFileDescriptor();
    MemoryBuffer mb;
    UnsignedArray parts;
    parts.append(0);
    fileDesc->serializeParts(mb, parts);

    // worker side
    IArrayOf<IPartDescriptor> partDescs;
    deserializePartFileDescriptors(mb, partDescs);
    IPartDescriptor &partDesc = partDescs.item(0);
    RemoteFilename rfn;
    partDesc.getFilename(0, rfn);
    StringBuffer path;
    rfn.getPath(path);
    Owned<IFile> iFile = createIFile(path);
    PROGLOG("File exists = %s", boolToStr(iFile->exists()));
}

static void remoteTest(const char *logicalName, bool withDali)
{
    CDfsLogicalFileName dlfn;
    dlfn.set(logicalName);
    logicalName = dlfn.get();
    StringBuffer svc, remoteName;
    CDfsLogicalFileName dlfn2;
    if (!dlfn.getRemoteSpec(svc, remoteName))
        remoteName.clear().append(logicalName);
    dlfn2.set(remoteName.str());
    remoteName.clear().append(dlfn2.get());

    PROGLOG("Reading file: %s, remoteName: %s", logicalName, remoteName.str());

    unsigned timeoutSecs = 60;
    unsigned keepAliveExpiryFrequency = 10;
    Owned<IUserDescriptor> userDesc = createUserDescriptor();
    userDesc->set("jsmith", "password");

    Owned<IDistributedFile> legacyDfsFile;
    if (dlfn.isRemote())
    {
        Owned<wsdfs::IDFSFile> dfsFile = wsdfs::lookupDFSFile(logicalName, AccessMode::readSequential, timeoutSecs, keepAliveExpiryFrequency, userDesc);

        if (dfsFile)
            legacyDfsFile.setown(createLegacyDFSFile(dfsFile));
    }
    else
    {
        if (!withDali)
            throw makeStringExceptionV(0, "remotetest for non-remote files needs Dali.");

        legacyDfsFile.setown(queryDistributedFileDirectory().lookup(dlfn, userDesc, AccessMode::tbdRead, false, false, nullptr, false));
    }

    if (!legacyDfsFile)
    {
        PROGLOG("Failed to open file: %s", logicalName);
        return;
    }

    testDFSFile(legacyDfsFile, remoteName);
}
