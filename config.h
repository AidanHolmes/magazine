#ifndef __MAGCONFIG_HDR
#define __MAGCONFIG_HDR

#include <stdio.h>
#include <exec/types.h>

#define MAG_MAX_SECTION_NAME    50
#define MAG_MAX_PARAMETER_NAME  50
#define MAG_MAX_PARAMETER_VALUE 100

#define MAG_OBJECT_PARAMETER 	1
#define MAG_OBJECT_TEXT			2

#define MAG_RETURN_NOERROR			0
#define MAG_RETURN_NOMEM			1
#define MAG_RETURN_FILE_ERROR		2
#define MAG_RETURN_PARAM_ERROR		3
#define MAG_RETURN_REPLAY			4

struct MagObject;
struct MagSection;
struct MagValue;
struct MagParameter;

struct MagObject
{
	struct MagSection *section;
	struct MagObject *next ;
	struct MagObject *prev;
	UWORD type;
};

struct MagValue
{
	struct MagParameter *parameter;
	struct MagValue *next;
	struct MagValue *prev;
	char szParameter[MAG_MAX_PARAMETER_NAME];
	char szValue[MAG_MAX_PARAMETER_VALUE];
};

struct MagParameter
{
	struct MagObject _obj;
	char szSectionID[MAG_MAX_SECTION_NAME];
	struct MagValue *valuePair;
};

struct MagText
{
	struct MagObject _obj;
	struct MagParameter *config; // could be null if no text config specified
	ULONG offset;
	UWORD length;
};

struct MagSection // 30 bytes
{
	UWORD id;
	ULONG offset;
	ULONG length;
	struct MagSection *parent;
	struct MagSection *child;
	struct MagSection *next;
	struct MagSection *prev;
	struct MagObject *firstObject;
};

typedef UWORD (*mag_object_callback)(struct MagObject *obj) ;

enum enConfigState {configStateSection, configStateParameter, configStateText};
enum enParamState {paramSetting, paramName, paramValue};

struct MagConfig{
	FILE *f;
	char *configbuffer;
	ULONG configlen;
	struct MagSection topSection;
	// config state variables
	mag_object_callback fn;
	enum enConfigState state;
	enum enParamState paramstate ;
	UWORD paramindex;
	struct MagSection *currentSection;
	struct MagObject *currentObject;
	struct MagValue *currentValue;
	UWORD lastid;
	ULONG offset;
	BOOL escaping ;
	BOOL quoting ;
};

UWORD parseConfig(struct MagConfig *config, mag_object_callback fn_callback);
void magInitialiseBuff(struct MagConfig *config, char *buffer, ULONG len);
UWORD magInitialiseFile(struct MagConfig *config, FILE *f, ULONG len);
void magCleanup(struct MagConfig *config);
struct MagSection *getLastSection(struct MagSection *s);
struct MagObject *getLastObject(struct MagSection *s);
struct MagValue *getLastValue(struct MagParameter *p);
struct MagText* findText(struct MagSection *section, struct MagObject *fromObj);
struct MagParameter* findParam(char *szName, struct MagSection *section, struct MagObject *fromObj);
struct MagParameter* reverseFindParam(char *szName, struct MagSection *section, struct MagObject *fromObj);
struct MagValue* findValue(char *szName, struct MagParameter *parameter);

UWORD magatouw(struct MagValue *v, UWORD defVal);
BOOL magstricmp(char *a, char *b, UWORD max);






#endif