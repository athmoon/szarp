/* 
  SZARP: SCADA software 

*/
/* 
 * IPK
 *
 * Pawe� Pa�ucha <pawel@praterm.com.pl>
 *
 * IPK 'season' class implementation.
 *
 * $Id$
 */

#include "conversion.h"
#include "szarp_config.h"
#include "liblog.h"

int TSSeason::parseXML(xmlTextReaderPtr reader) {

//TODO: remove all printf
printf("name seasons: parseXML\n");

	bool isEmpty = false;

	XMLWrapper xw(reader);

	if (xw.IsEmptyTag())
		isEmpty = true;

	const char* need_attr[] = { "month_start", "day_start", "month_end", "day_end", 0 };
	xw.AreValidAttr(need_attr);

	const char* ignored_tags[] = { "#text", "#comment", 0 };
	xw.SetIgnoredTags(ignored_tags);

	for (bool isAttr = xw.IsFirstAttr(); isAttr == true; isAttr = xw.IsNextAttr()) {
		const xmlChar* attr = xw.GetAttr();
		const xmlChar* attr_name = xw.GetAttrName();

//TODO: make it better - atoi or map with pointers to function
		if (xw.IsAttr("month_start")) {
			if (sscanf((const char*) attr, "%d", &defs.month_start) != 1) {
				xw.XMLError("Invalid start date of default summer season definition");
				return 1;
			}
		} else
		if (xw.IsAttr("day_start")) {
			if (sscanf((const char*) attr, "%d", &defs.day_start) != 1) {
				xw.XMLError("Invalid start date of default summer season definition");
				return 1;
			}
		} else
		if (xw.IsAttr("month_end")) {
			if (sscanf((const char*) attr, "%d", &defs.month_end) != 1) {
				xw.XMLError("Invalid end date of default summer season definition");
				return 1;
			}
		} else
		if (xw.IsAttr("day_end")) {
			if (sscanf((const char*) attr, "%d", &defs.day_end) != 1) {
				xw.XMLError("Invalid end date of default summer season definition");
				return 1;
			}
		} else {
			printf("<seasons> not known attr: %s\n",attr_name);
//			assert(0 == 1 && "not known attr");
		}
	} // FORALLATTR

	if (isEmpty)
		return 0;

	xw.NextTag();

	for (;;) {

		if(xw.IsTag("season")) {
			if (xw.IsBeginTag()) {

				int year;
				Season s;

				XMLWrapper xwseason(reader);
				const char* need_attr_seasn[] = {"year", "month_start",  "month_end", "day_start", "day_end", 0 };

				xwseason.AreValidAttr(need_attr_seasn);

				for (bool isAttr = xwseason.IsFirstAttr(); isAttr == true; isAttr = xwseason.IsNextAttr()) {
					const xmlChar* attr = xwseason.GetAttr();
					const xmlChar* attr_name = xwseason.GetTagName();

					if (xwseason.IsAttr("year")) {
						if (sscanf((const char*) attr, "%d", &year) != 1) {
							xwseason.XMLError("Invalid year definition");
							return 1;
						}
					} else
					if (xwseason.IsAttr("month_start")) {
						if (sscanf((const char*) attr, "%d", &s.month_start) != 1) {
							xwseason.XMLError("Invalid start date of summer season definition");
							return 1;
						}
					} else
					if (xwseason.IsAttr("day_start")) {
						if (sscanf((const char*) attr, "%d", &s.day_start) != 1) {
							xwseason.XMLError("Invalid start date of summer season definition");
						return 1;
						}
					} else
					if (xwseason.IsAttr("month_end")) {
						if (sscanf((const char*) attr, "%d", &s.month_end) != 1) {
							xwseason.XMLError("Invalid end date of summer season definition");
							return 1;
						}
					} else
					if (xwseason.IsAttr("day_end")) {
						if (sscanf((const char*) attr, "%d", &s.day_end) != 1) {
							xwseason.XMLError("Invalid end date of summer season definition");
							return 1;
						}
					} else {
						printf("not known attr=%s\n",attr_name);
						assert(0 == 1 && "not known attr");
					}
				} // FORALLATTR

				seasons[year] = s;

			}
			xw.NextTag();
		} else
		if(xw.IsTag("seasons")) {
			break;
		}
		else {
//TODO: make it better
			const xmlChar* name = xw.GetTagName();
			printf("ERROR: not know name = %s\n",name);
			assert(name == NULL && "not know name");
		}
	}

printf("name seasons parseXML END\n");

	return 0;

}

int TSSeason::parseXML(xmlNodePtr node) {

	char *c = NULL;

#define NOATR(p, n) \
	{ \
	sz_log(1, "XML parsing error: attribute '%s' in node '%s' not\
 found (line %ld)", \
 			n, p->name, xmlGetLineNo(p)); \
			return 1; \
	}
#define NEEDATR(p, n) \
	if (c) free(c); \
	c = (char *)xmlGetNoNsProp(p, (xmlChar *)n); \
	if (!c) NOATR(p, n);
#define X (xmlChar*)

	NEEDATR(node, X"month_start");
	if (sscanf(c, "%d", &defs.month_start) != 1) {
		sz_log(1, "Invalid start date of default summer season definition (line: %ld)", 
			xmlGetLineNo(node));
		free(c);
		return 1;
	}

	NEEDATR(node, X"day_start");
	if (sscanf(c, "%d", &defs.day_start) != 1) {
		sz_log(1, "Invalid start date of default summer season definition (line: %ld)", 
			xmlGetLineNo(node));
		free(c);
		return 1;
	}

	NEEDATR(node, X"month_end");
	if (sscanf(c, "%d", &defs.month_end) != 1) {
		sz_log(1, "Invalid end date of default summer season definition (line: %ld)", 
			xmlGetLineNo(node));
		free(c);
		return 1;

	}

	NEEDATR(node, X"day_end");
	if (sscanf(c, "%d", &defs.day_end) != 1) {
		sz_log(1, "Invalid end date of default summer season definition (line: %ld)", 
			xmlGetLineNo(node));
		free(c);
		return 1;

	}

	for (node = node->children; node; node = node->next) {
		if (strcmp((char*)node->name, "season"))
			continue;

		int year;
		NEEDATR(node, X"year");
		if (sscanf(c, "%d", &year) != 1) {
			sz_log(1, "Invalid year definition (line: %ld)", 
				xmlGetLineNo(node));
			free(c);
			return 1;
		}

		Season s;
		NEEDATR(node, X"month_start");
		if (sscanf(c, "%d", &s.month_start) != 1) {
			sz_log(1, "Invalid start date of summer season definition (line: %ld)", 
				xmlGetLineNo(node));
			free(c);
			return 1;
		}

		NEEDATR(node, X"day_start");
		if (sscanf(c, "%d", &s.day_start) != 1) {
			sz_log(1, "Invalid start date of summer season definition (line: %ld)", 
				xmlGetLineNo(node));
			free(c);
			return 1;
		}

		NEEDATR(node, X"month_end");
		if (sscanf(c, "%d", &s.month_end) != 1) {
			sz_log(1, "Invalid end date of summer season definition (line: %ld)", 
				xmlGetLineNo(node));
			free(c);
			return 1;
		}

		NEEDATR(node, X"day_end");
		if (sscanf(c, "%d", &s.day_end) != 1) {
			sz_log(1, "Invalid end date of summer season definition (line: %ld)", 
				xmlGetLineNo(node));
			free(c);
			return 1;
		}
		free(c);
		c = NULL;

		seasons[year] = s;

	}

	return 0;

#undef X
#undef NEEDATR
#undef NOATR

}

xmlNodePtr TSSeason::generateXMLNode() const {
#define ITOA(x) snprintf(buffer, sizeof(buffer), "%d", x)
#define X (const xmlChar*) 

	char buffer[30];
	xmlNodePtr r = xmlNewNode(NULL, X"seasons");

	ITOA(defs.month_start);
	xmlSetProp(r, X"month_start", SC::A2U(buffer).c_str());

	ITOA(defs.day_start);
	xmlSetProp(r, X"day_start", SC::A2U(buffer).c_str());

	ITOA(defs.month_end);
	xmlSetProp(r, X"month_end", SC::A2U(buffer).c_str());

	ITOA(defs.day_end);
	xmlSetProp(r, X"day_end", SC::A2U(buffer).c_str());

	for (tSeasons::const_iterator i = seasons.begin();
			i != seasons.end();
			++i) {

		int year = i->first;
		const Season& s = i->second;

		xmlNodePtr n = xmlNewNode(NULL, X"season");

		ITOA(year);
		xmlSetProp(n, X"year", SC::A2U(buffer).c_str());

		ITOA(s.month_start);
		xmlSetProp(r, X"month_start", SC::A2U(buffer).c_str());

		ITOA(s.day_start);
		xmlSetProp(r, X"day_start", SC::A2U(buffer).c_str());

		ITOA(s.month_end);
		xmlSetProp(r, X"month_end", SC::A2U(buffer).c_str());

		ITOA(s.day_end);
		xmlSetProp(r, X"day_end", SC::A2U(buffer).c_str());

		xmlAddChild(r, n);
	}

	return r;

}

bool TSSeason::CheckSeason(const Season &s, int month, int day) const {
	bool result = false;

	if ((s.month_start < month || (s.month_start == month && s.day_start <= day))
		&& (s.month_end > month || (s.month_end == month && s.day_end > day)))
		result = true;

	return result;

}

const TSSeason::Season& TSSeason::GetSeason(int year) const {
	tSeasons::const_iterator i = seasons.find(year);

	if (i != seasons.end())
		return (*i).second;
	else
		return defs;
}

bool TSSeason::IsSummerSeason(int year, int month, int day) const {
	const Season& s = GetSeason(year);
	return CheckSeason(s, month, day);
}

bool TSSeason::IsSummerSeason(time_t time) const {
	struct tm *t;
#ifndef HAVE_LOCALTIME_R
	t = localtime(&time);
#else
	struct tm ptm;
	t = localtime_r(&time, &ptm);
#endif
	return IsSummerSeason(t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);
}

