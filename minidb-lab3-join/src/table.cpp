#include "minidb/table.hpp"

std::string DataTypeToString(DataType dt) {
	switch (dt) {
	case INT:
		return "INT";
	case FLOAT:
		return "FLOAT";
	case STRING:
		return "STRING";
	case BOOL:
		return "BOOL";
	}
	return "UNKNOWN";
}

DataType StringToDataType(const std::string& t) {
	if (t == "INT")
		return DataType::INT;
	else if (t == "FLOAT")
		return DataType::FLOAT;
	else if (t == "BOOL")
		return DataType::BOOL;
	else {
		return DataType::STRING;
	}
}

std::optional<Value> convert_string_to_type(const std::string& str, DataType type) {
	try {
		switch (type) {
		case DataType::INT:
			return (std::stoi(str));
		case DataType::FLOAT:
			return (std::stof(str));
		case DataType::BOOL:
			if (str == "true")
				return true;
			if (str == "false")
				return false;
			break;
		case DataType::STRING:
			return str;
		}
	} catch (...) {
		return std::nullopt;
	}
	return std::nullopt;
}
