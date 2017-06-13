#include <iostream>
#include <vector>
#include <regex>
#include <fstream>
#include <unordered_set>

using namespace std;

#define ERR_INVALID_ARG 1
#define ERR_NO_INPUT 2
#define ERR_INPUT_READ 4
#define ERR_OUTUPT_WRITE 8
#define IC_INDENT 0
#define IC_NULL 0
#define IC_FUNCTION 1
#define IC_STRING 2
#define IC_SINGLE_APO 2
#define IC_DOUBLE_APO 3
#define IC_COMMENT 4
#define IC_INLINE_COMMENT 4
#define IC_BLOCK_COMMENT 5

ofstream out;
vector<string> inputs;
vector<string> outputs;

regex isLua(".lua$");
regex isRequire("require\\(.+\\)");
regex isValidRequire("require\\(\"[^\"]+\"\\)|require\\('[^']+'\\)");
regex isPath("\\/");
regex getPath("^(.*\\/)[^/]+$");
regex getName("^(?:.*\\/)?([^/]+?)(?:\\.lua$|$)");

int depthMax = 32;
int mode = 0;
bool uniq = false;
bool quiet = false;
bool depthLimit = false;
unordered_set<string> names;

void parseFile(string name, string path, string indent, int isComment, int depth) {
    if (depthLimit) return;
    bool indentOnly = (isComment == IC_INDENT), isSingleApo = (isComment == IC_SINGLE_APO), isDoubleApo = (isComment == IC_DOUBLE_APO), isInlineComment = (isComment == IC_INLINE_COMMENT), isBlock = (isComment == IC_BLOCK_COMMENT);
    try {
        if (!quiet) cout << endl;
        for (int i = 0; i < depth; i++) {
            if (!quiet) cout << "  ";
        }
        if (!quiet) cout << "- " << name;
        if (depth > depthMax) {
            depthLimit = true;
            if (!quiet) cout << ": Depth limit reached"; else cout << "Depth limit reached in " << name << endl;
            return;
        }
        if (uniq) {
            if (isComment == IC_NULL && names.find(name) != names.end()) {
                if (!quiet) cout << ": already in";
                return;
            }
            names.insert(name);
        }

        ifstream in(path + name + ".lua", ifstream::in);
        if (in.fail()) {
            if (isBlock || isSingleApo || isDoubleApo) {
                out << "(require error)";
            } else if (indentOnly) {
                out << "print(\"(require error)\")";
            } else {
                out << "--[[ (require error) --]]";
            }
            if (!quiet) cout << ": error " << strerror(errno) << endl; else cout << "Error in " << name << ": " << strerror(errno) << endl;
            return;
        }
        string line = "";
        int numLines = 0;
        string currentIndent = "";
        while (!in.eof()) {
            ++numLines;
            getline(in, line);
            currentIndent = indent;
            indentOnly = (isComment == IC_INDENT || isComment == IC_BLOCK_COMMENT);
            isInlineComment = (isComment == IC_INLINE_COMMENT);
            int indChar = 0;
            while (line[indChar] == ' ' || line[indChar] == '\t') {
                if (line[indChar] == '\t') {
                    currentIndent += "    ";
                } else {
                    currentIndent += " ";
                }
                ++indChar;
            }
            if (numLines>1 && (isComment == IC_BLOCK_COMMENT || isComment == IC_INDENT)) out << currentIndent;
            for (int i = indChar; i < line.size(); i++) {
                if (isComment == IC_BLOCK_COMMENT) {
                    if (i+3 < line.size()) {
                        if (line.substr(i, 4) == "--[[") {
                            indentOnly = false;
                            out << "<comment>";
                            i += 3;
                            continue;
                        } else if (line.substr(i, 4) == "--]]") {
                            indentOnly = false;
                            out << "<end comment>";
                            i += 3;
                            continue;
                        }
                    }
                } else {
                    if (!isInlineComment && i+1 < line.size() && line.substr(i, 2) == "--") {
                        indentOnly = false;
                        if (i+3 < line.size()) {
                            if (line.substr(i+2, 2) == "[[") {
                                isBlock = true;
                                out << "--[[";
                                i += 3;
                                continue;
                            } else if (line.substr(i+2, 2) == "]]") {
                                isBlock = false;
                                out << "--]]";
                                i += 3;
                                continue;
                            }
                        }
                        if (!isBlock) {
                            if (isComment == IC_FUNCTION) {
                                break;
                            } else {
                                out << "--";
                                isInlineComment = true;
                                i += 1;
                                continue;
                            }
                        }
                    } else if (!isBlock && !isInlineComment) {
                        if (line[i] == '"') {
                            indentOnly = false;
                            if (isComment == IC_DOUBLE_APO) {
                                out << "\\\"";
                                continue;
                            } else {
                                isDoubleApo = !isDoubleApo;
                                out << "\"";
                                continue;
                            }
                        } else if (line[i] == '\'') {
                            indentOnly = false;
                            if (isComment == IC_SINGLE_APO) {
                                out << "\\\'";
                                continue;
                            } else {
                                isSingleApo = !isSingleApo;
                                out << "\'";
                                continue;
                            }
                        } else if (line[i] == '\\') {
                            indentOnly = false;
                            if (isComment & IC_STRING) {
                                out << "\\\\";
                                continue;
                            } else {
                                out << "\\";
                                continue;
                            }
                        }
                    }
                }
                if (i+10<line.size() && line.substr(i, 8) == "require(") {
                    string filename = "";
                    char par = line[i+8];
                    int last = i+9;
                    bool closed = false;
                    while ((last < line.size() - 2) && (line[last] != par)) {
                        filename += line[last];
                        if (line[last] == ')') {
                            closed = false;
                            --last;
                            break;
                        }
                        if (line[++last]==par) {
                            closed = true;
                        }
                    }
                    if (line[last+1] != ')') closed = false;
                    if (!closed || (par == '\'' && isSingleApo) || (par == '"' && isDoubleApo)) {
                        out << "require(";
                        i += 7;
                        continue;
                    }
                    i = last+1;
                    smatch mtc;
                    regex_match(filename, mtc, getPath);
                    string localpath = path + mtc[1].str();
                    regex_match(filename, mtc, getName);
                    string localname = mtc[1].str();
                    string localindent = currentIndent;
                    int typecode = isSingleApo * IC_SINGLE_APO + isDoubleApo * IC_DOUBLE_APO + isBlock * IC_BLOCK_COMMENT + isInlineComment * IC_INLINE_COMMENT;
                    if (typecode == 0) typecode = (!indentOnly || i+1 < line.size()) * IC_FUNCTION;
                    parseFile(localname, localpath, localindent, typecode, depth+1);
                    if (!quiet) cout << " (ID" << typecode << ")";
                } else {
                    indentOnly = false;
                    out << line[i];
                }
            }
            if (!in.eof()) {
                if (isComment & IC_STRING) {
                    out << "\\n";
                } else if (isComment == IC_INLINE_COMMENT || isComment == IC_FUNCTION) {
                    out << " ";
                } else {
                    out << endl;
                }
            }
        }
    } catch ( ... ) {
        if (isBlock || isSingleApo || isDoubleApo) {
            out << "(require error)";
        } else if (indentOnly) {
            out << "print(\"(require error)\")";
        } else {
            out << "--[[ (require error) --]]";
        }
        if (!quiet) cout << ": error"; else cout << "Error in " << name << endl;
    }
}

void displayHelp() {
    cout << "LUA Merger v0.01" << endl;
    cout << "(c) Discookie - Released under the MIT license" << endl << endl;
    cout << "Usage:" << endl;
    cout << endl;
    cout << "  luam [args] <input> [<output>] [<input2> <output2>]..." << endl;
    cout << "  luam -i [args] <input> [<input2> [<input3>]]..." << endl;
    cout << "  luam -s [args] <input> [<input2>]...";
    cout << endl;
    cout << "Parameters:" << endl;
    cout << "  input  - The file you want to be merged" << endl;
    cout << "  output - The file you want to merge into" << endl;
    cout << "           Default: <input>_o.lua" << endl;
    cout << endl;
    cout << "Arguments:" << endl;
    cout << "  -h, --help:   Displays this message" << endl;
    cout << "  -u, --usage:  Displays usage in LUA files" << endl;
    cout << "  -q, --quiet:  Quiet mode, only output errors" << endl;
    cout << "  -d, --depth:  Depth limit, to prevent infinite loops [integer]" << endl;
    cout << "  -i, --input:  Input only mode, everything into separated files" << endl;
    cout << "  -s, --single: Single file mode, merges everything into one file" << endl;
    cout << "  -o, --output: Specify output for single file mode" << endl;
    cout << "  -n, --unique: Only insert files once (does not affect inline requires)" << endl;
    cout << endl;
    cout << "Refer to the wiki for docs" << endl;
}

void displayUsage() {
    cout << "LUA Merger v0.01" << endl;
    cout << "(c) Discookie - Released under the MIT license" << endl << endl;
    cout << "Usage in LUA files:" << endl;
    cout << endl;
    cout << "  require(\"file\")" << endl;
    cout << "  require('file')" << endl;
    cout << "    - Relative path to file" << endl;
    cout << endl;
    cout << "  require(\"path/to/file.lua\")" << endl;
    cout << "    - Can parse subdirectories" << endl;
    cout << "      - Relative to current file path, forward slash only" << endl;
    cout << "    - Can end in .lua" << endl;
    cout << endl;
    cout << "  local _var = require(\"onelinefunc\") * 2" << endl;
    cout << "    - Inline functions" << endl;
    cout << "    - Newlines are replaced with spaces" << endl;
    cout << endl;
    cout << "  func(), --require(\"notparsed\")" << endl;
    cout << "    - Inline comments are not parsed" << endl;
    cout << endl;
    cout << "  --[[ require(\"parsed\") --]]" << endl;
    cout << "    - Multiline comments are parsed" << endl;
    cout << "    - Newlines are kept" << endl;
    cout << "    - Comment ends are replaced" << endl;
    cout << endl;
    cout << "  print(\"require(\\\"string\\\")\")" << endl;
    cout << "    - This is not parsed" << endl;
    cout << endl;
    cout << "  print(\"require('output')\")" << endl;
    cout << "    - This is parsed" << endl;
    cout << endl;
    cout << "Refer to the wiki for more examples" << endl;
}

int main(int argc, char* argv[]) {
    if (argc==0) {
        displayHelp();
        return 0;
    }
    vector<string> args(argv + 1, argv + argc);
    bool inputDefined = false;
    int i = 0;
    int outPos = -1;
    while (i < args.size()) {
        if (args[i][0] == '-') {
            if (args[i] == "-o" || args[i] == "--output"){
                outPos = ++i;
            }
            else if (args[i] == "-i" || args[i] == "--input") {
                if (mode) {
                    cout << "Cannot use both input and single mode!";
                    return ERR_INVALID_ARG;
                }
                mode = 1;
            }
            else if (args[i] == "-s" || args[i] == "--single") {
                if (mode) {
                    cout << "Cannot use both input and single mode!";
                    return ERR_INVALID_ARG;
                }
                mode = 2;
            }
            else if (args[i] == "-n" || args[i] == "--uniq") {
                uniq = true;
            }
            else if (args[i] == "-q" || args[i] == "--quiet") {
                quiet = true;
            }
            else if (args[i] == "-d" || args[i] == "--depth") {
                ++i;
                try {
                    depthMax = stoi(args[i]);
                } catch (invalid_argument) {
                    cout << "Invalid argument: " << args[i-1] << " " << args[i];
                }
            }
            else if (args[i] == "-u" || args[i] == "--usage") {
                displayUsage();
                return 0;
            }
            else if (args[i] == "-h" || args[i] == "-?" || args[i] == "--help") {
                displayHelp();
                return 0;
            }
            else {
                cout << "Invalid argument: " << args[i] << endl;
                return ERR_INVALID_ARG;
            }
        }
        i++;
    }
    if (!quiet) cout << "LUA Merger v0.01" << endl;
    if (!quiet) cout << "(c) Discookie - Released under the MIT license" << endl << endl;
    if (outPos != -1 && mode != 2) {
        cout << "Invalid argument: " << args[outPos-1] << endl;
        return ERR_INVALID_ARG;
    }
    if (mode == 0) {
        if (!quiet) cout << "Regular mode" << endl << endl;
        bool inputNext = true;
        for (int i = 0; i < args.size(); i++) {
            if (args[i][0] == '-') continue;
            if (inputNext) {
                inputDefined = true;
                inputNext = false;
                inputs.push_back(args[i]);
            } else {
                inputNext = true;
                outputs.push_back(args[i]);
            }
        }
        if (!inputDefined) {
            cout << "No input file specified!" << endl;
            return ERR_NO_INPUT;
        }
        if (!inputNext) {
            smatch nom, pth;
            regex_match(inputs[inputs.size()-1], nom, getName);
            regex_match(inputs[inputs.size()-1], pth, getPath);
            if (!quiet) cout << "Auto-generating" << (inputs.size()>0?" last ":" ") << "output file as '" << nom[1].str() << "_o.lua'" << endl;
            outputs.push_back(pth[1].str() + nom[1].str() + "_o");
        }
        smatch name;
        smatch path;
        smatch oname;
        smatch opath;
        for (int i = 0; i < inputs.size(); i++) {
            try {
                depthLimit = false;
                names = unordered_set<string>();
                regex_match(inputs[i], name, getName);
                regex_match(inputs[i], path, getPath);
                regex_match(outputs[i], oname, getName);
                regex_match(outputs[i], opath, getPath);
                out.open(opath[1].str() + oname[1].str() + ".lua");
                if (out.fail()) {
                    cout << "Error processing " << i+1 << ". output (" << outputs[i] << "): " << strerror(errno) << endl;
                    continue;
                }
                parseFile(name[1].str(), path[1].str(), "", IC_NULL, 0);
                out.close();
            } catch (...) {
                cout << "Error processing " << i+1 << ". file (" << inputs[i] << ", " << outputs[i] << ")" << endl;
            }
        }
    }
    else if (mode == 1) {
        if (!quiet) cout << "Input-only mode" << endl << endl;
        for (int i = 0; i < args.size(); i++) {
            if (args[i][0] == '-') continue;
            inputDefined = true;
            inputs.push_back(args[i]);
        }
        if (!inputDefined) {
            cout << "No input file specified!" << endl;
            return ERR_NO_INPUT;
        }
        for (int i = 0; i < inputs.size(); i++) {
            depthLimit = false;
            names = unordered_set<string>();
            smatch name;
            smatch path;
            regex_match(inputs[i], name, getName);
            regex_match(inputs[i], path, getPath);
            out.open(path[1].str()+name[1].str()+"_o.lua");
            if (out.fail()) {
                cout << "Error processing " << i+1 << ". output (" << outputs[i] << "): " << strerror(errno) << endl;
                break;
            }
            parseFile(name[1].str(), path[1].str(), "", IC_NULL, 0);
            out.close();
        }
    }
    else if (mode == 2) {
        if (!quiet) cout << "Single-output mode" << endl << endl;
        if (outPos == args.size() || args[outPos][0] == '-') {
            cout << "Invalid argument: " << args[outPos-1] << " <missing>" << endl;
            return ERR_INVALID_ARG;
        }
        for (int i = 0; i < args.size(); i++) {
            if (args[i][0] == '-' || i == outPos) continue;
            inputDefined = true;
            inputs.push_back(args[i]);
        }
        if (!inputDefined) {
            cout << "No input file specified!" << endl;
            return ERR_NO_INPUT;
        }
        smatch oname;
        smatch opath;
        string oStr;
        if (outPos == -1) {
            regex_match(inputs[0], oname, getName);
            regex_match(inputs[0], opath, getPath);
            oStr = opath[1].str() + oname[1].str() + "_o.lua";
            if (!quiet) cout << "No output file given, auto-generating '" << oname[1].str() << ".lua'."  << endl;
        } else {
            regex_match(args[outPos], oname, getName);
            regex_match(args[outPos], opath, getPath);
            oStr = opath[1].str() + oname[1].str() + ".lua";
        }
        out.open(oStr);
        for (int i = 0; i < inputs.size(); i++) {
            depthLimit = false;
            names = unordered_set<string>();
            smatch name;
            smatch path;

            regex_match(inputs[i], name, getName);
            regex_match(inputs[i], path, getPath);
            parseFile(name[1].str(), path[1].str(), "", IC_NULL, 0);
        }
        out.close();
    }
    return 0;
}
