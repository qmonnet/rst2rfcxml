﻿// Copyright (c) Dave Thaler
// SPDX-License-Identifier: MIT

#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include "..\external\CLI11.HPP"
#include "rst2rfcxml.h"

using namespace std;

// Remove whitespace from beginning and end of string.
static string _trim(string s) {
	regex e("^\\s+|\\s+$");
	return regex_replace(s, e, "");
}

static string _anchor(string value)
{
	const string legal_first_character = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_:";
	const string legal_anchor_characters = legal_first_character + "1234567890-.";
	string anchor = "";
	for (size_t i = 0; i < value.length(); i++) {
		char c = value[i];
		if (legal_anchor_characters.find(c) == string::npos) {
			// Drop disallowed characters.
			continue;
		}
		if (anchor.empty() && legal_first_character.find(c) == string::npos) {
			// Drop disallowed start characters.
			continue;
		}
		if (isupper(c)) {
			c = tolower(c);
		}
		anchor += c;
	}
	return anchor;
}

// Output XML header.
void rst2rfcxml::output_header(ostream& output_stream)
{
	output_stream << R"(<?xml version="1.0" encoding="UTF-8"?>
  <?xml-stylesheet type="text/xsl" href="rfc2629.xslt"?>
  <!-- generated by https://github.com/dthaler/rst2rfcxml version 0.1 -->

<!DOCTYPE rfc [
]>

<?rfc rfcedstyle="yes"?>
<?rfc toc="yes"?>
<?rfc tocindent="yes"?>
<?rfc sortrefs="yes"?>
<?rfc symrefs="yes"?>
<?rfc strict="yes"?>
<?rfc comments="yes"?>
<?rfc inline="yes"?>
<?rfc text-list-symbols="-o*+"?>
<?rfc docmapping="yes"?>

)";

	output_stream << format("<rfc ipr=\"{}\" docName=\"{}\" category=\"{}\" submissionType=\"{}\">", _ipr, _document_name, _category, _submission_type) << endl << endl;
	_contexts.push(xml_context::RFC);
	output_stream << "  <front>" << endl;
	_contexts.push(xml_context::FRONT);
}

static void _output_optional_attribute(ostream& output_stream, string name, string value)
{
	if (!value.empty()) {
		output_stream << format(" {}=\"{}\"", name, value);
	}
}

void rst2rfcxml::output_authors(ostream& output_stream) const
{
	for (auto author : _authors) {
		output_stream << format("<author fullname=\"{}\"", author.fullname);
		_output_optional_attribute(output_stream, "initials", author.initials);
		_output_optional_attribute(output_stream, "surname", author.surname);
		_output_optional_attribute(output_stream, "role", author.role);
		output_stream << "></author>" << endl;
	}
}

void rst2rfcxml::pop_context(ostream& output_stream)
{
	switch (_contexts.top()) {
	case xml_context::ABSTRACT:
		output_stream << "</abstract>" << endl;
		break;
	case xml_context::BACK:
		output_stream << "</back>" << endl;
		break;
	case xml_context::DEFINITION_DESCRIPTION:
		output_stream << "</dd>" << endl;
		break;
	case xml_context::DEFINITION_LIST:
		output_stream << "</dl>" << endl;
		break;
	case xml_context::DEFINITION_TERM:
		output_stream << "</dt>" << endl;
		break;
	case xml_context::FRONT:
		output_stream << "</front>" << endl;
		break;
	case xml_context::LIST_ELEMENT:
		output_stream << "</li>" << endl;
		break;
	case xml_context::MIDDLE:
		output_stream << "</middle>" << endl;
		break;
	case xml_context::RFC:
		output_stream << "</rfc>" << endl;
		break;
	case xml_context::SECTION:
		output_stream << "</section>" << endl;
		break;
	case xml_context::TABLE:
		output_stream << "</table>" << endl;
		break;
	case xml_context::TABLE_BODY:
		output_stream << "</tbody>" << endl;
		break;
	case xml_context::TABLE_HEADER:
		output_stream << "</tr></thead>" << endl;
		break;
	case xml_context::TEXT:
		output_stream << "</t>" << endl;
		break;
	case xml_context::UNORDERED_LIST:
		output_stream << "</ul>" << endl;
		break;
	default:
		break;
	}
	_contexts.pop();
}

// Pop all XML contexts until we are down to a specified XML level.
void rst2rfcxml::pop_contexts(int level, ostream& output_stream)
{
	while (_contexts.size() > level) {
		pop_context(output_stream);
	}
}

// Replace occurrences of ``foo`` with <tt>foo</tt>.
string _replace_constant_width_instances(string line)
{
	size_t index;
	while ((index = line.find("``")) != string::npos) {
		size_t next_index = line.find("``", index + 2);
		if (next_index == string::npos) {
			break;
		}
		string before = line.substr(0, index);
		string middle = line.substr(index + 2, next_index - index - 2);
		string after = line.substr(next_index + 2);
		line = format("{}<tt>{}</tt>{}", before, _trim(middle), after);
	}
	return line;
}

// Given a string, replace all occurrences of a given substring.
static string _replace_all(string line, string from, string to)
{
	size_t index;
	size_t start = 0;
	while ((index = line.find(from, start)) != string::npos) {
		line.replace(index, from.length(), to);
		start = index + to.length();
	}
	return line;
}

static string _replace_internal_links(string line)
{
	size_t start;
	while ((start = line.find("`")) != string::npos) {
		size_t end = line.find("`_", start + 1);
		if (end == string::npos) {
			break;
		}
		string before = line.substr(0, start);
		string middle = line.substr(start + 1, end - start - 1);
		string after = line.substr(end + 2);
		line = format("{}<xref target=\"{}\"/>{}", before, _anchor(middle), after);
	}
	return line;
}

// Handle escapes.
static string _handle_escapes(string line)
{
	// Trim whitespace.
	line = _trim(line);

	// Unescape things RST requires to be escaped.
	line = _replace_all(line, "\\*", "*");
	line = _replace_all(line, "\\|", "|");

	// Escape things XML requires to be escaped.
	line = _replace_all(line, "&", "&amp;");
	line = _replace_all(line, "<", "&lt;");
	line = _replace_all(line, ">", "&gt;");

	line = _replace_constant_width_instances(line);
	line = _replace_internal_links(line);

	return line;
}

constexpr int BASE_SECTION_LEVEL = 2; // <rfc><front/middle/back>.

static bool _handle_variable_initialization(string line, string label, string& field)
{
	string prefix = format(".. |{}| replace:: ", _trim(label));
	if (line.starts_with(prefix)) {
		field = _handle_escapes(line.substr(prefix.length()));
		return true;
	}
	return false;
}

// Handle variable initializations. Returns true if input has been handled.
bool rst2rfcxml::handle_variable_initializations(string line)
{
	if (_handle_variable_initialization(line, "category", _category) ||
		_handle_variable_initialization(line, "docName", _document_name) ||
		_handle_variable_initialization(line, "ipr", _ipr) ||
		_handle_variable_initialization(line, "submissionType", _submission_type) ||
		_handle_variable_initialization(line, "titleAbbr", _abbreviated_title)) {
		return true;
	}

	// Handle author field initializations.
	string author_fullname_prefix = ".. |authorFullname| replace:: ";
	if (line.starts_with(author_fullname_prefix)) {
		author author;
		author.fullname = _trim(line.substr(author_fullname_prefix.length()));
		_authors.push_back(author);
		return true;
	}
	string author_role_prefix = ".. |authorRole| replace:: ";
	if (line.starts_with(author_role_prefix)) {
		author& author = _authors.back();
		author.role = _trim(line.substr(author_role_prefix.length()));
		return true;
	}
	string author_surname_prefix = ".. |authorSurname| replace:: ";
	if (line.starts_with(author_surname_prefix)) {
		author& author = _authors.back();
		author.surname = _trim(line.substr(author_surname_prefix.length()));
		return true;
	}
	string author_initials_prefix = ".. |authorInitials| replace:: ";
	if (line.starts_with(author_initials_prefix)) {
		author& author = _authors.back();
		author.initials = _trim(line.substr(author_initials_prefix.length()));
		return true;
	}
	return false;
}

// Perform table handling.
bool rst2rfcxml::handle_table_line(string line, ostream& output_stream)
{
	// Process column definitions.
	if (line.find_first_not_of(" ") != string::npos &&
		line.find_first_not_of(" =") == string::npos) {
		if (in_context(xml_context::TABLE_BODY)) {
			pop_context(output_stream); // TABLE_BODY
			pop_context(output_stream); // TABLE
			_column_indices.clear();
			return true;
		}
		if (in_context(xml_context::TABLE_HEADER)) {
			pop_context(output_stream); // TABLE_HEADER
			output_stream << " <tbody>" << endl;
			_contexts.push(xml_context::TABLE_BODY);
			return true;
		}

		while (in_context(xml_context::TEXT) ||
			   in_context(xml_context::DEFINITION_DESCRIPTION) ||
			   in_context(xml_context::DEFINITION_LIST)) {
			pop_context(output_stream);
		}
		output_stream << "<table><thead><tr>" << endl;
		_contexts.push(xml_context::TABLE);
		_contexts.push(xml_context::TABLE_HEADER);

		// Find column indices.
		size_t index = line.find_first_of("=");
		while (index != string::npos) {
			_column_indices.push_back(index);
			index = line.find_first_not_of("=", index);
			if (index == string::npos) {
				break;
			}
			index = line.find_first_of("=", index);
		}
		return true;
	}

	// Process a table header line.
	if (in_context(xml_context::TABLE_HEADER)) {
		for (int column = 0; column < _column_indices.size(); column++) {
			size_t start = _column_indices[column];
			size_t count = (column + 1 < _column_indices.size()) ? _column_indices[column + 1] - start : -1;
			if (line.length() > start) {
				string value = _handle_escapes(line.substr(start, count));
				output_stream << format("  <th>{}</th>", value) << endl;
			}
		}
		return true;
	}

	// Process a table body line.
	if (in_context(xml_context::TABLE_BODY)) {
		output_stream << " <tr>" << endl;
		for (int column = 0; column < _column_indices.size(); column++) {
			size_t start = _column_indices[column];
			size_t count = (column + 1 < _column_indices.size()) ? _column_indices[column + 1] - start : -1;
			string value;
			if (line.length() >= start) {
				value = _handle_escapes(line.substr(start, count));
			}
			output_stream << format("  <td>{}</td>", value) << endl;
		}
		output_stream << " </tr>" << endl;
		return true;
	}

	return false;
}

// Handle document and section titles.
bool rst2rfcxml::handle_title_line(string line, ostream& output_stream)
{
	if (line.starts_with("=") && line.find_first_not_of("=", 0) == string::npos) {
		if (in_context(xml_context::TITLE)) {
			_contexts.pop(); // End of title.
			return true;
		}

		// Title marker begins after a blank line.
		if (_previous_line.find_first_not_of(" ") == string::npos) {
			_contexts.push(xml_context::TITLE);
			return true;
		}

		// Previous line is a L1 section heading.
		pop_contexts(BASE_SECTION_LEVEL, output_stream);
		if (in_context(xml_context::FRONT)) {
			pop_contexts(1, output_stream);
			output_stream << "<middle>" << endl;
			_contexts.push(xml_context::MIDDLE);
		}
		string title = _handle_escapes(_previous_line);
		output_stream << format("<section anchor=\"{}\" title=\"{}\">", _anchor(title), title) << endl;
		_contexts.push(xml_context::SECTION);
		_previous_line.clear();
		return true;
	}
	if (line.starts_with("-") && line.find_first_not_of("-", 0) == string::npos) {
		// Previous line is a L2 section heading.
		pop_contexts(BASE_SECTION_LEVEL + 1, output_stream);
		string title = _handle_escapes(_previous_line);
		output_stream << format("<section anchor=\"{}\" title=\"{}\">", _anchor(title), title) << endl;
		_contexts.push(xml_context::SECTION);
		_previous_line.clear();
		return true;
	}
	if (line.starts_with("~") && line.find_first_not_of("~", 0) == string::npos) {
		// Previous line is a L3 section heading.
		pop_contexts(BASE_SECTION_LEVEL + 2, output_stream);
		string title = _handle_escapes(_previous_line);
		output_stream << format("<section anchor=\"{}\" title=\"{}\">", _anchor(title), title) << endl;
		_contexts.push(xml_context::SECTION);
		_previous_line.clear();
		return true;
	}
	if (in_context(xml_context::TITLE)) {
		output_stream << format("<title abbrev=\"{}\">{}</title>", _abbreviated_title, line) << endl;
		return true;
	}
	return false;
}

// Process a new line of input.
void rst2rfcxml::process_line(string line, ostream& output_stream)
{
	if (line == ".. contents::") {
		// Include table of contents.
		// This is already the default in rfc2xml.
		return;
	}
	if (line == ".. sectnum::") {
		// Number sections.
		// This is already the default in rfc2xml.
		return;
	}
	if (line == ".. header::") {
		output_header(output_stream);
		return;
	}

	// Title lines must be handled before table lines.
	if (handle_title_line(line, output_stream)) {
		return;
	}

	// Handle tables first, where escapes must be dealt with per
	// cell, in order to preserve column locations.
	if (handle_table_line(line, output_stream)) {
		return;
	}

	line = _handle_escapes(line);

	if (handle_variable_initializations(line)) {
		return;
	}

	// Handle definition lists.
	if (line.starts_with("  ") && (line.find_first_not_of(" ") != string::npos) &&
		!_previous_line.empty() && isalpha(_previous_line[0])) {
		if (!in_context(xml_context::DEFINITION_LIST)) {
			output_stream << "<dl>";
			_contexts.push(xml_context::DEFINITION_LIST);
		}
    	output_stream << "<dt>";
		_contexts.push(xml_context::DEFINITION_TERM);
	}
	output_previous_line(output_stream);
	_previous_line = line;
}

bool rst2rfcxml::in_context(xml_context context) const
{
	return (!_contexts.empty() && _contexts.top() == context);
}

// Output the previous line.
void rst2rfcxml::output_previous_line(ostream& output_stream)
{
	if (_previous_line.starts_with("* ")) {
		if (in_context(xml_context::LIST_ELEMENT)) {
			pop_context(output_stream);
		}
		if (!in_context(xml_context::UNORDERED_LIST)) {
			output_stream << "<ul>" << endl;
			_contexts.push(xml_context::UNORDERED_LIST);
		}
		output_stream << format("<li>{}", _trim(_previous_line.substr(2))) << endl;
		_contexts.push(xml_context::LIST_ELEMENT);
	} else if (_previous_line.find_first_not_of(" ") != string::npos) {
		if (in_context(xml_context::DEFINITION_TERM) && _previous_line.starts_with("  ")) {
			pop_context(output_stream);
			output_stream << "<dd>";
			_contexts.push(xml_context::DEFINITION_DESCRIPTION);
		} else if (!in_context(xml_context::DEFINITION_DESCRIPTION) &&
			!in_context(xml_context::DEFINITION_TERM) &&
			!in_context(xml_context::TEXT)) {
			if (in_context(xml_context::FRONT)) {
				output_authors(output_stream);
				output_stream << "<abstract>" << endl;
				_contexts.push(xml_context::ABSTRACT);
			}
			if (in_context(xml_context::DEFINITION_LIST)) {
				pop_context(output_stream);
			}
			output_stream << "<t>" << endl;
			_contexts.push(xml_context::TEXT);
		}
		output_stream << _trim(_previous_line) << endl;
	}

	if (_previous_line.find_first_not_of(" ") == string::npos) {
		// End any contexts that end at a blank line.
		while (!_contexts.empty()) {
			if (in_context(xml_context::DEFINITION_DESCRIPTION) ||
				in_context(xml_context::LIST_ELEMENT) ||
				in_context(xml_context::TEXT) ||
				in_context(xml_context::UNORDERED_LIST)) {
				pop_context(output_stream);
			} else {
				break;
			}
		}
	}
}

// Process all lines in an input stream.
void rst2rfcxml::process_input_stream(istream& input_stream, ostream& output_stream)
{
	string line;
	while (getline(input_stream, line)) {
		process_line(line, output_stream);
	}
	process_line({}, output_stream);
}

// Process multiple input files that contribute to an output file.
void rst2rfcxml::process_files(vector<string> input_filenames, ostream& output_stream)
{
	for (auto input_filename : input_filenames) {
		ifstream input_file(input_filename);
		process_input_stream(input_file, output_stream);
	}
	pop_contexts(0, output_stream);
}

int main(int argc, char** argv)
{
	CLI::App app{ "A reStructured Text to xmlrfc Version 3 converter" };
	string output_filename;
	app.add_option("-o", output_filename, "Output filename");
	vector<string> input_filenames;
	app.add_option("-i,input", input_filenames, "Input filenames")->mandatory(true);
	CLI11_PARSE(app, argc, argv);

	rst2rfcxml rst2rfcxml;
	if (output_filename.empty()) {
		rst2rfcxml.process_files(input_filenames, cout);
	} else {
		ofstream outfile(output_filename);
		rst2rfcxml.process_files(input_filenames, outfile);
	}
	return 0;
}
