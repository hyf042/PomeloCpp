#ifndef _POMELOCPP_JSON_H_
#define _POMELOCPP_JSON_H_

#include "pomelo.h"
#include "Lib/json/json.h"

namespace PomeloCpp
{
	class JsonHelper
	{
	public:
		static Json::Value translate(json_t *json) {
			switch(json_typeof(json))
			{
			case JSON_OBJECT:
			{
				Json::Value obj;
				const char *key;
				json_t *value;
				json_object_foreach(json, key, value) {
					obj[key] = translate(value);
				}
				return obj;
			}
			case JSON_ARRAY:
			{
				Json::Value obj;
				int size = json_array_size(json);
				for (int i = 0; i < size; i++) {
					obj[i] = translate(json_array_get(json, i));
				}
				return obj;
			}
			case JSON_STRING:
				return json_string_value(json);
			case JSON_INTEGER:
				return static_cast<int>(json_integer_value(json));
			case JSON_REAL:
				return json_real_value(json);
			case JSON_TRUE:
				return true;
			case JSON_FALSE:
				return false;
			case JSON_NULL:
			default:
				return NULL;
			}
		}

		static json_t* translate(Json::Value value) {
			switch(value.type()) {
			case Json::booleanValue:
				if (value.asBool())
					return json_true();
				else
					return json_false();
			case Json::stringValue:
				return json_string(value.asCString());
			case Json::intValue:
				return json_integer(value.asInt());
			case Json::realValue:
				return json_real(value.asDouble());
			case Json::arrayValue:
			{
				json_t *t = json_array();
				for (int i = 0; i < value.size(); i++)
					json_array_append_new(t, translate(value[i]));
				return t;
			}
			case Json::objectValue:
			{
				json_t *t = json_object();
				Json::Value::Members members = value.getMemberNames();
				for(Json::Value::Members::iterator ptr = members.begin(); ptr != members.end(); ptr++)
					json_object_set_new(t, (*ptr).c_str(), translate(value[*ptr]));
				return t;
			}
			case Json::nullValue:
			default:
				return json_null();
			}
		}
	};
}

#endif