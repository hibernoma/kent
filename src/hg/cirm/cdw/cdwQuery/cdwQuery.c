/* cdwQuery - Get list of tagged files.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "cheapcgi.h"
#include "cdw.h"
#include "cdwLib.h"
#include "rql.h"
#include "tagStorm.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cdwQuery - Get list of tagged files.\n"
  "usage:\n"
  "   cdwQuery 'sql-like query'\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

static char *lookupField(void *record, char *key)
/* Lookup a field in a tagStanza. */
{
struct tagStanza *stanza = record;
return tagFindVal(stanza, key);
}

boolean statementMatch(struct rqlStatement *rql, struct tagStanza *stanza,
	struct lm *lm)
/* Return TRUE if where clause and tableList in statement evaluates true for tdb. */
{
struct rqlParse *whereClause = rql->whereClause;
if (whereClause == NULL)
    return TRUE;
else
    {
    struct rqlEval res = rqlEvalOnRecord(whereClause, stanza, lookupField, lm);
    res = rqlEvalCoerceToBoolean(res);
    return res.val.b;
    }
}

int matchCount = 0;
boolean doSelect = FALSE;

void traverse(struct tagStorm *tags, struct tagStanza *list, 
    struct rqlStatement *rql, struct lm *lm)
/* Recursively traverse stanzas on list. */
{
struct tagStanza *stanza;
for (stanza = list; stanza != NULL; stanza = stanza->next)
    {
    if (stanza->children)
	traverse(tags, stanza->children, rql, lm);
    else    /* Just apply query to leaves */
	{
	if (statementMatch(rql, stanza, lm))
	    {
	    ++matchCount;
	    if (doSelect)
		{
		struct slName *field;
		for (field = rql->fieldList; field != NULL; field = field->next)
		    {
		    char *val = tagFindVal(stanza, field->name);
		    if (val != NULL)
			printf("%s\t%s\n", field->name, val);
		    }
		printf("\n");
		}
	    }
	}
    }
}

void cdwQuery(char *rqlQuery)
/* cdwQuery - Get list of tagged files.. */
{
struct lineFile *lf = lineFileOnString("query", TRUE, cloneString(rqlQuery));
struct sqlConnection *conn = cdwConnect();
struct lm *lm = lmInit(0);
struct rqlStatement *rql = rqlStatementParse(lf);
int stormCount = slCount(rql->tableList);
if (stormCount != 1)
    errAbort("Can only handle one tag storm file in query, got %d", stormCount);
char *tagsFileName = rql->tableList->name;

struct tagStorm *tags = tagStormFromFile(tagsFileName);
struct hash *hash = tagStormUniqueIndex(tags, "meta");

/* Get list of all files including a bunch of fields we'll add to tagStorm.
 * The files will end up as new stanzas underneath the stanza that matches the
 * meta column. */
char query[2048];
sqlSafef(query, sizeof(query), 
	"select %s from cdwFile,cdwValidFile where cdwFile.id=cdwValidFile.fileId "
	"order by cdwFile.id ",
	    "experiment meta,tags,cdwFileName,licensePlate accession,size,md5,format,"
	    "submitFileName submit_file_name,"
	    "outputType output,itemCount item_count,"
	    "mapRatio map_ratio,depth,part file_part,pairedEnd paired_end ");
int metaIx = 0, tagsIx = 1, cdwFileNameIx = 2;
struct sqlResult *sr = sqlGetResult(conn, query);
char *fieldNames[200];
int fieldIx;
for (fieldIx=0; fieldIx < ArraySize(fieldNames); ++fieldIx)
    {
    char *field = sqlFieldName(sr);
    if (field == NULL)
	break;
    fieldNames[fieldIx] = field;
    }
int fieldCount = fieldIx;
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *joiner = row[metaIx];
    char *cgiTags = row[tagsIx];
    char *cdwFileName = row[cdwFileNameIx];
    struct tagStanza *parent = hashFindVal(hash, joiner);
    if (parent != NULL)
	{
	struct tagStanza *stanza = tagStanzaNewAtEnd(tags, parent);
	char fileName[PATH_LEN];
	safef(fileName, sizeof(fileName), "%s%s", cdwRootDir, cdwFileName);
	tagStanzaAdd(tags, stanza, "file", fileName);
	int rowIx=0;
	for (rowIx = 0; rowIx < fieldCount; ++rowIx)
	    {
	    char *name = fieldNames[rowIx];
	    if (rowIx != 1)  // avoid tags column
		{
		char *val = row[rowIx];
		if (!isEmpty(val))
		    tagStanzaAdd(tags, stanza, name, val);
		}
	    }
	struct cgiParsedVars *cgis = cgiParsedVarsNew(cgiTags);
	struct cgiVar *var;
	for (var = cgis->list; var != NULL; var = var->next)
	    {
	    if (!isEmpty(var->val))
		tagStanzaAdd(tags, stanza, var->name, var->val);
	    }
	cgiParsedVarsFree(&cgis);
	slReverse(&stanza->tagList);
	}
    }
sqlFreeResult(&sr);

/* Get list of all tag types in tree and use it to expand wildcards in the query
 * field list. */
struct slName *allFieldList = tagTreeFieldList(tags);
rql->fieldList = wildExpandList(allFieldList, rql->fieldList, TRUE);

/* Traverse tag tree outputting when rql statement matches */
doSelect = sameWord(rql->command, "select");
traverse(tags, tags->forest, rql, lm);
if (sameWord(rql->command, "count"))
    printf("%d\n", matchCount);
tagStormFree(&tags);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
cdwQuery(argv[1]);
return 0;
}