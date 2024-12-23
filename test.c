#include <stdio.h>
#include <exec/types.h>
#include "config.h"

UWORD magcallback(struct MagObject *obj)
{
	struct MagParameter *p;
	struct MagTest *t ;
	struct MagValue *v;
	
	printf("Callback object %u\n", obj->type);
	if (obj->type == MAG_OBJECT_PARAMETER){
		p = (struct MagParameter*)obj ;
		printf("Parameter: %s in section ID %u, Attributes:\n", p->szSectionID, p->_obj.section->id);
		for(v=p->valuePair; v; v=v->next){
			printf("Name: %s, Value: %s\n", v->szParameter, v->szValue);
		}
	}
	return 0;
}

int main(int argc, char **argv)
{
	FILE *f = NULL ;
	struct MagConfig mconf;
	UWORD ret = 0;
	
	f=fopen("config", "r");
	if (!f){
		printf("Cannot open config file\n");
		return 10;
	}
	
	if (magInitialiseFile(&mconf, f, 0) != 0){
		printf("Failed to initialise mag config\n");
		return 10;
	}
	
	ret = parseConfig(&mconf, magcallback);
	if (ret != 0){
		printf("parseConfig failed with code %u\n", ret);
	}else{
		printf("parseConfig succeeded\n") ;
	}
	
	magCleanup(&mconf);
	
	return 0;
}
