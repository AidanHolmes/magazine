#include "config.h"
#include "debug.h"
#include <string.h>
#include <proto/exec.h>
#include <exec/exec.h>

////////////////////////////////////////////////////
//
// Private functions
//

void _addValueHead(struct MagParameter *p, struct MagValue *v)
{
	// Set new head on old object
	if (v->parameter->valuePair = v){
		v->parameter->valuePair = v->next ; // could be null
	}
	
	// stitch any prev and next values from exiting parameter list
	if (v->prev){
		v->prev->next = v->next;
	}
	if (v->next){
		v->next->prev = v->prev;
	}
	
	// Clear prev and next links - isolate this node
	v->prev = v->next = NULL;
	
	// Move any existing head value to the next value in the link list
	if (p->valuePair){
		p->valuePair->prev = v;
		v->next = p->valuePair ;
	}
	p->valuePair = v; // set new head
		
}

void _deleteObject(struct MagObject *o)
{
	if (o->section->firstObject == o){
		o->section->firstObject = o->next;
	}
	if (o->prev){
		o->prev->next = o->next;
	}
	if (o->next){
		o->next->prev = o->prev;
	}
	
	FreeVec(o);
	D(printf("FreeVec: _deleteObject %p\n", o));
}

void _freeObjs(struct MagObject* o)
{
	struct MagParameter* p = NULL;
	struct MagObject* object, *tobj = NULL;
	struct MagValue *v, *tv = NULL;
	
	if (!o){
		return;
	}
	if (o->prev){
		// tie off the end of the list for preceeding objects
		o->prev->next = NULL;
	}
	// Free up all objects following and including the input parameter
	for(object=o;object;){
		tobj = object->next ;
		if(object->type == MAG_OBJECT_PARAMETER){
			p = (struct MagParameter*)object;
			if (p->valuePair){
				// Free value pairs
				for(v=p->valuePair; v;){
					tv = v->next;
					FreeVec(v);
					D(printf("FreeVec: _freeObjs Value %p\n", v));
					v = tv;
				}
			}
		}
		FreeVec(object);
		D(printf("FreeVec: _freeObjs Object %p\n", object));
		object = tobj;
	}
}

void _freeSection(struct MagSection *s)
{
	struct MagSection *section = NULL, *nexts = NULL ;
	
	if (!s){
		return;
	}
	if (s->child){
		_freeSection(s->child);
	}
	for (section=s;section;){
		nexts = section->next;
		if (section->firstObject){
			_freeObjs(section->firstObject);
		}
		FreeVec(section);
		D(printf("FreeVec: _freeSection %p\n", section));
		section = nexts; 
	}
}

void _magInitSection(struct MagConfig *config, struct MagSection *s)
{
	memset(s, 0, sizeof(struct MagSection));
	s->id = config->lastid++ ;
}

void _magInitialise(struct MagConfig *config)
{
	memset(config, 0, sizeof(struct MagConfig));
	_magInitSection(config, &config->topSection);
	config->state = configStateSection;
}

struct MagSection *_createSection(struct MagConfig *config, struct MagSection *parent)
{
	struct MagSection *attachTo = NULL, *newSection = NULL;
	
	newSection = (struct MagSection *)AllocVec(sizeof(struct MagSection), MEMF_ANY);
	D(printf("Allocated Memory:_createSection %p, length %u\n", newSection, sizeof(struct MagSection)));
	if (!newSection){
		return NULL;
	}
	
	_magInitSection(config, newSection);

	if (!parent->child){
		parent->child = newSection;
	}else{
		attachTo = getLastSection(parent->child);
		newSection->prev = attachTo;
		if (attachTo){
			attachTo->next = newSection;
		}
	}

	newSection->parent = parent;
	return newSection;
}

void _magInitParameter(struct MagConfig *config, struct MagParameter *p)
{
	memset(p, 0, sizeof(struct MagParameter));
	p->_obj.type = MAG_OBJECT_PARAMETER;
}

void _linkToSection(struct MagSection *parent, struct MagObject *obj)
{
	struct MagObject *attachTo = NULL ;
	
	attachTo = getLastObject(parent);
	obj->prev = attachTo ; // could be null if first.
	if (attachTo){
		attachTo->next = obj;
	}else{
		// first object in section
		parent->firstObject = obj;
	}
	obj->section = parent ;
}

struct MagParameter *_createParameter(struct MagConfig *config, struct MagSection *parent)
{
	struct MagParameter *newParameter = NULL;
		
	newParameter = (struct MagParameter *)AllocVec(sizeof(struct MagParameter), MEMF_ANY);
	D(printf("Allocated Memory:_createParameter %p, length %u\n", newParameter, sizeof(struct MagParameter)));
	if (!newParameter){
		return NULL;
	}

	_magInitParameter(config, newParameter);
	_linkToSection(parent, (struct MagObject*)newParameter);
	
	return newParameter;
}

void _magInitText(struct MagConfig *config, struct MagText *p)
{
	memset(p, 0, sizeof(struct MagText));
	p->_obj.type = MAG_OBJECT_TEXT;
}

struct MagText* _createText(struct MagConfig *config, struct MagSection *parent)
{
	struct MagText *newText = NULL ;
	
	newText = (struct MagText *)AllocVec(sizeof(struct MagText), MEMF_ANY);
	D(printf("Allocated Memory:_createText %p, length %u\n", newText, sizeof(struct MagText)));
	if (!newText){
		return NULL;
	}
	_magInitText(config, newText);
	_linkToSection(parent, (struct MagObject*)newText);
	
	return newText;
}

void _magInitValue(struct MagValue *v)
{
	memset(v, 0, sizeof(struct MagValue));
}

void _linkToParameter(struct MagParameter *p, struct MagValue* v)
{
	struct MagValue *attachTo = NULL ;
	
	if (!p->valuePair){
		// First to be added to parameter
		p->valuePair = v;
	}else{
		attachTo = getLastValue(p);
		attachTo->next = v;
		v->prev = attachTo ;
	}
}

struct MagValue* _createValue(struct MagParameter *param)
{
	struct MagValue* newValue = NULL;
	
	newValue = (struct MagValue*)AllocVec(sizeof(struct MagValue), MEMF_ANY);
	D(printf("Allocated Memory:_createValue %p, length %u\n", newValue, sizeof(struct MagValue)));
	if (!newValue){
		return NULL ;
	}
	_magInitValue(newValue) ;
	_linkToParameter(param, newValue);
	
	return newValue ;
}

UWORD _parseParameter(struct MagConfig *config, char c)
{
	struct MagValue *thisValue = NULL ;
	struct MagParameter *thisParam = NULL ;
	
	thisParam = (struct MagParameter*)config->currentObject;
	thisValue = (struct MagValue*)config->currentValue;
	
	switch(c){
		case '"':
			// quoting
			config->quoting = !config->quoting;
			break;
		case ']':
			if (!config->quoting){
				// close parameter parsing
				if (config->fn){
					config->fn((struct MagObject*)thisParam);
				}
				config->state = configStateSection;
				break;
			}
			// else fall through to default
			goto fallthrough;
		case ' ':
		case '\t':
		case '\r':
		case '\n':
			// white text terminates a param value or setting. 
			if (!config->quoting){
				if (config->paramstate == paramSetting || config->paramstate == paramValue){
					config->paramstate = paramName;
					config->paramindex = 0;
					// create new param
					thisValue = _createValue(thisParam);
					if (!thisValue){
						return MAG_RETURN_NOMEM;
					}
					config->currentValue = thisValue ;
				}
				break;
			}
			//else fall through
		default:
fallthrough:
			if (c == '=' && !config->quoting){
				if (config->paramstate == paramName){
					config->paramstate = paramValue;
					config->paramindex = 0;
				}
			}else{
				// any char
				switch(config->paramstate){
					case paramSetting:
						if (config->paramindex < MAG_MAX_SECTION_NAME){
							thisParam->szSectionID[config->paramindex++] = c;
						}
						break;
					case paramName:
						if (config->paramindex < MAG_MAX_PARAMETER_NAME){
							thisValue->szParameter[config->paramindex++] = c;
						}
						break;
					case paramValue:
						if (config->paramindex < MAG_MAX_PARAMETER_VALUE){
							thisValue->szValue[config->paramindex++] = c;
						}
						break;
					default:
						return MAG_RETURN_PARAM_ERROR;
				}
			}		
	}		
	
	return MAG_RETURN_NOERROR ;
}

UWORD _parseText(struct MagConfig *config, char c)
{
	struct MagText *thisText = NULL ;
	
	switch(c){
		case '\\': //escape - process next as a text char
			config->escaping = TRUE ;
			break;
		case '{':
		case '}':
		case '[':
			if (!config->escaping){
				// Send char back to new section parsing
				config->state = configStateSection;
				
				// save state of text
				thisText = (struct MagText*)config->currentObject ;
				if (config->f){
					thisText->length = ftell(config->f) - thisText->offset - 1;
				}else{
					thisText->length = config->offset - thisText->offset;
				}
				// Look for any previouly set TEXT parameter for this text and set the parameter against the text object
				thisText->config = reverseFindParam("TEXT", thisText->_obj.section, (struct MagObject*)thisText) ;
				
				if (config->fn){
					config->fn((struct MagObject*)thisText);
				}
				return MAG_RETURN_REPLAY;					
			}
			config->escaping = FALSE ;
			break;
		default:
			// continue processing as text
			break;
	}		
	
	return MAG_RETURN_NOERROR ;
}

UWORD _parseSection(struct MagConfig *config, char c)
{
	struct MagSection *newSection = NULL ;
	struct MagParameter *newParameter = NULL ;
	struct MagText *newText = NULL ;
	
	switch(c){
		case '}':
			// Save length of section
			if (config->f){
				config->currentSection->length = ftell(config->f) - config->currentSection->offset - 1;
			}else{
				config->currentSection->length = config->offset - config->currentSection->offset;
			}
			if (config->currentSection->parent){
				config->currentSection = config->currentSection->parent;
			}
			break;
		case '{': // new section
			newSection = _createSection(config, config->currentSection);
			if (!newSection){
				return MAG_RETURN_NOMEM;
			}
			config->currentSection = newSection;
			// Save offset for section
			if (config->f){
				config->currentSection->offset = ftell(config->f);
			}else{
				config->currentSection->offset = config->offset + 1;
			}
			
			config->state = configStateSection; // redundant as we are already in this state
			break;
		case '[':
			newParameter = _createParameter(config, config->currentSection);
			if (!newParameter){
				return MAG_RETURN_NOMEM;
			}
			config->currentObject = (struct MagObject*)newParameter;
			config->state = configStateParameter;
			config->paramstate = paramSetting; // reset state for new parameter parsing
			config->paramindex = 0;
			break;
		case ' ':
		case '\t':
		case '\n':
		case '\r':
			break ; // white space - ignore
		default:
			newText = _createText(config, config->currentSection);
			if (!newText){
				return MAG_RETURN_NOMEM;
			}
			if (config->f){
				newText->offset = ftell(config->f)-1;
			}else{
				newText->offset = config->offset;
			}
			config->currentObject = (struct MagObject*)newText;
			config->state = configStateText;
			break;
	}
	return MAG_RETURN_NOERROR;
}

////////////////////////////////////////////////////
//
// Public functions
//
#define _conftoupper(x) ((x >= 'a' && x <= 'z')?x-32:x)
BOOL magstricmp(char *a, char *b, UWORD max)
{
	UWORD i = 0;
	
	for(;i< (max-1) && a[i] != '\0';i++){
		if (_conftoupper(a[i]) != _conftoupper(b[i])){
			return FALSE ;
		}
	}
	if (a[i] == '\0' && b[i] == '\0'){
		return TRUE;
	}
	return FALSE;
}

UWORD magatouw(struct MagValue *v, UWORD defVal)
{
	UWORD val = 0;
	char *s = NULL ;
	
	if (!v) return defVal;
	
    for(s=v->szValue; *s; s++){
        if ((*s >= '0') && (*s <= '9')) {
            val *= 10;
            val += (*s - '0');
        }else{
			return defVal;
		}
    }
    return val;
}

void magInitialiseBuff(struct MagConfig *config, char *buffer, ULONG len)
{
	_magInitialise(config);
	config->configbuffer = buffer;
	config->configlen = len;
	config->topSection.offset = 0;
	config->topSection.length = len ;
}

UWORD magInitialiseFile(struct MagConfig *config, FILE *f, ULONG len)
{
	ULONG curpos = 0;
	
	_magInitialise(config);
	if (len == 0){
		curpos = ftell(f);
		if (fseek(f,0,SEEK_END) != 0){
			return MAG_RETURN_FILE_ERROR ;
		}
		len = ftell(f) - curpos;
		if (fseek(f,curpos,SEEK_SET) != 0){
			return MAG_RETURN_FILE_ERROR ;
		}
	}
	config->configlen = len ;
	config->f = f;
	config->topSection.offset = curpos;
	config->topSection.length = len ;
	
	return MAG_RETURN_NOERROR ;
}

struct MagSection *getLastSection(struct MagSection *s)
{
	struct MagSection *p = NULL;
	for(p=s; p->next != NULL; p=p->next);
	
	return p;
}

struct MagObject *getLastObject(struct MagSection *s)
{
	struct MagObject *p = NULL ;
	if (!s->firstObject){
		return NULL;
	}
	for(p=s->firstObject; p->next != NULL; p=p->next);
	
	return p;
}

struct MagValue *getLastValue(struct MagParameter *p)
{
	struct MagValue *v = NULL ;
	if (!p->valuePair){ // No values in parameter
		return NULL;
	}
	for(v=p->valuePair; v->next != NULL; v=v->next);
	
	return v;
}

struct MagText* findText(struct MagSection *section, struct MagObject *fromObj)
{
	struct MagObject *i = NULL;
	
	if (!fromObj){
		fromObj = section->firstObject;
	}else{
		fromObj = fromObj->next; // could be null
	}
	
	for (i=fromObj; i; i=i->next){
		if (i->type == MAG_OBJECT_TEXT){
			// Match found
			return (struct MagText*)i ;
		}
	}
	return NULL ;
}

struct MagParameter* findParam(char *szName, struct MagSection *section, struct MagObject *fromObj)
{
	struct MagObject *i = NULL;
	
	if (!fromObj){
		fromObj = section->firstObject;
	}else{
		fromObj = fromObj->next; // could be null
	}
	
	for (i=fromObj; i; i=i->next){
		if (i->type == MAG_OBJECT_PARAMETER){
			if (magstricmp(szName, ((struct MagParameter*)i)->szSectionID, MAG_MAX_SECTION_NAME)){
				// Match found
				return (struct MagParameter* )i ;
			}
		}
	}
	return NULL ;
}

struct MagParameter* reverseFindParam(char *szName, struct MagSection *section, struct MagObject *fromObj)
{
	struct MagObject *i = NULL;
	
	if (!fromObj){
		fromObj = section->firstObject;
		if (fromObj){
			for (i=fromObj; i->next; i=i->next); // Got to last entry in list of objects
		}
	}else{
		fromObj = fromObj->prev; // could be null
	}
	
	for (i=fromObj; i; i=i->prev){
		if (i->type == MAG_OBJECT_PARAMETER){
			if (magstricmp(szName, ((struct MagParameter*)i)->szSectionID, MAG_MAX_SECTION_NAME)){
				// Match found
				return (struct MagParameter*)i ;
			}
		}
	}
	return NULL ;
}

struct MagValue* findValue(char *szName, struct MagParameter *parameter)
{
	struct MagValue *i = NULL;
	for (i=(struct MagValue*)parameter->valuePair; i; i=(struct MagValue*)i->next){
		if (magstricmp(szName, i->szParameter, MAG_MAX_PARAMETER_NAME)){
			// Match found
			return i ;
		}
	}
	return NULL ;
}

UWORD parseConfig(struct MagConfig *config, mag_object_callback fn_callback)
{
	char c;
	UWORD ret = MAG_RETURN_NOERROR ;
	
	// initialise with top section
	config->currentSection = &config->topSection;
	config->fn = fn_callback;
	
	for(config->offset = 0;config->offset < config->configlen;config->offset++){
		if (config->f){
			if (fread(&c,1,1,config->f) != 1){
				if (feof(config->f) == 0){
					// To do: this closes the top level section (and all remaining sections)
					// Run callback
					return MAG_RETURN_NOERROR ;
				}else{
					return MAG_RETURN_FILE_ERROR ; //error occurred
				}
			}
		}else if(config->configbuffer){
			c = config->configbuffer[config->offset];
		}else{
			// No data - error parsing
			return MAG_RETURN_PARAM_ERROR;
		}
replay:
		switch(config->state){
			case configStateSection:
				ret = _parseSection(config, c);
				break;
			case configStateParameter:
				ret = _parseParameter(config,c);
				break;
			case configStateText:
				ret = _parseText(config,c);
				break;
			default:
				break;
		}
		if (ret == MAG_RETURN_REPLAY){
			goto replay;
		}
		if (ret != MAG_RETURN_NOERROR){
			return ret;
		}
	}
	
	// No more characters to process, but should we close out existing text?
	if (config->state == configStateText){
		_parseText(config,'}'); // bit of a hack, but make it think end of section read
	}
	
	return ret ;
}

void magCleanup(struct MagConfig *config)
{
	struct MagSection *s;
	
	s = &config->topSection;
	
	if (s->child){
		_freeSection(s->child);
	}
	
	if (s->firstObject){
		_freeObjs(s->firstObject);
	}
}
