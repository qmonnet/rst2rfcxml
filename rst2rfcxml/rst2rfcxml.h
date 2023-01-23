﻿// rst2rfcxml.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <iostream>
#include <stack>

enum class xml_context {
    ABSTRACT,
    BACK,
    BLOCKQUOTE,
    DEFINITION_LIST,
    DEFINITION_TERM,
    DEFINITION_DESCRIPTION,
    FRONT,
    LIST_ELEMENT,
    MIDDLE,
    RFC,
    SECTION,
    SOURCE_CODE,
    TABLE,
    TABLE_HEADER,
    TABLE_BODY,
    TEXT,
    TITLE,
    UNORDERED_LIST,
};

struct author {
    std::string initials;
    std::string surname;
    std::string fullname;
    std::string role;
};

struct reference {
    std::string anchor;
    std::string title;
    std::string rst_target;
    std::string xml_target;
    std::string type;
    int use_count;
};

class rst2rfcxml {
public:
    void process_files(std::vector<std::string> input_filenames, std::ostream& output_stream);

private:
    void process_input_stream(std::istream& input_stream, std::ostream& output_stream);
    void process_line(std::string current, std::string next, std::ostream& output_stream);
    void output_line(std::string line, std::ostream& output_stream);
    void output_header(std::ostream& output_stream);
    void output_back(std::ostream& output_stream);
    void output_references(std::ostream& output_stream, std::string type, std::string title);
    void output_authors(std::ostream& output_stream) const;
    void pop_context(std::ostream& output_stream);
    void pop_contexts(int level, std::ostream& output_stream);
    void pop_contexts_until(xml_context end, std::ostream& output_stream);
    bool in_context(xml_context context) const;
    bool handle_variable_initializations(std::string line);
    bool handle_table_line(std::string current, std::string next, std::ostream& output_stream);
    bool handle_title_line(std::string current, std::string next, std::ostream& output_stream);
    bool handle_section_title(int level, std::string marker, std::string current, std::string next, std::ostream& output_stream);
    std::string replace_links(std::string line);
    std::string handle_escapes_and_links(std::string line);

    std::string _document_name;
    std::string _ipr;
    std::string _category;
    std::vector<size_t> _column_indices;
    std::vector<author> _authors;
    std::string _submission_type;
    std::string _abbreviated_title;
    std::stack<xml_context> _contexts;
    bool _source_code_skip_blank_lines;

    // Some RST markup modifies the previous line, so we need to
    // keep track of the previous line and process it only after
    // we know whether the next one affects it.
    std::string _previous_line;
    
    std::map<std::string, reference> _rst_references;
    std::map<std::string, std::string> _xml_references;
};
