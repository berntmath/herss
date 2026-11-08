/********************************************************************************
Project:      The Hydraulic Economic River System Simulator (HERSS)
Filename:     outputformats.cpp

This software is released under the MIT license.
********************************************************************************/

#include "outputformats.h"

#include <cctype>
#include <cstdio>
#include <set>
#include <utility>


namespace {

struct NodeFileInfo {
    std::vector<std::string> names;   // Column names without the four date columns.
    std::vector<std::string> units;   // Same length as names, or empty when the file has no units line.
    std::string txt_name;             // File names relative to the output directory.
    std::string csv_name;
    bool has_txt = false;
    bool has_csv = false;
};

typedef std::vector<std::vector<std::string> > Rows;

std::vector<std::string> SplitTokens(const std::string &line) {
    std::istringstream in(line);
    std::vector<std::string> tokens;
    std::string t;
    while (in >> t) tokens.push_back(t);
    return tokens;
}

bool IsDateHeader(const std::vector<std::string> &tok) {
    return tok.size() >= 5 && tok[0] == "yyyy" && tok[1] == "mm" && tok[2] == "dd" && tok[3] == "hh";
}

std::string StripBrackets(const std::string &s) {
    if (s.size() >= 2 && s.front() == '[' && s.back() == ']') return s.substr(1, s.size() - 2);
    return s;
}

// The node writers differ in their preamble, but all of them write the column names on the last
// line starting with "yyyy mm dd hh", optionally preceded by a units line of the same kind.
bool ReadNodeTxt(const std::string &path, NodeFileInfo &info, Rows &rows) {
    std::ifstream in(path.c_str());
    if (!in.is_open()) return false;

    std::vector<std::vector<std::string> > headers;
    std::string line;
    bool in_data = false;
    size_t bad_rows = 0;

    while (std::getline(in, line)) {
        std::vector<std::string> tok = SplitTokens(line);
        if (tok.empty()) continue;

        if (!in_data) {
            if (IsDateHeader(tok)) {
                headers.push_back(tok);
                continue;
            }
            if (!isdigit(static_cast<unsigned char>(tok[0][0]))) continue;  // Preamble
            if (headers.empty()) return false;
            in_data = true;
            info.names.assign(headers.back().begin() + 4, headers.back().end());
            if (headers.size() >= 2 && headers[headers.size() - 2].size() == headers.back().size()) {
                const std::vector<std::string> &u = headers[headers.size() - 2];
                for (size_t i = 4; i < u.size(); i++) info.units.push_back(StripBrackets(u[i]));
            }
        }

        if (tok.size() != info.names.size() + 4) { bad_rows++; continue; }
        rows.push_back(tok);
    }

    if (!in_data) return false;
    if (bad_rows > 0) {
        LOG_WARN("Skipped " + std::to_string(bad_rows) + " rows with an unexpected number of columns in " + path);
    }
    return true;
}

bool WriteNodeCsv(const std::string &path, const NodeFileInfo &info, const Rows &rows) {
    std::ofstream out(path.c_str());
    if (!out.is_open()) return false;

    out << "datetime";
    for (size_t c = 0; c < info.names.size(); c++) out << "," << info.names[c];
    out << "\n";

    char stamp[32];
    for (size_t r = 0; r < rows.size(); r++) {
        snprintf(stamp, sizeof(stamp), "%04d-%02d-%02dT%02d:00",
                 atoi(rows[r][0].c_str()), atoi(rows[r][1].c_str()),
                 atoi(rows[r][2].c_str()), atoi(rows[r][3].c_str()));
        out << stamp;
        for (size_t c = 4; c < rows[r].size(); c++) out << "," << rows[r][c];
        out << "\n";
    }
    return true;
}

std::string JsonString(const std::string &s) {
    std::string out = "\"";
    for (size_t i = 0; i < s.size(); i++) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);  // UTF-8 bytes are valid JSON as they are.
                }
        }
    }
    return out + "\"";
}

// Labels are the topology keywords that define each link.
std::vector<std::pair<std::string, size_t> > CollectLinks(Node *node, size_t nr_nodes) {
    std::vector<std::pair<std::string, size_t> > links;
    // Unset values are NOT_INIT or a negative number cast to size_t, and both are >= nr_nodes.
    #define ADD_LINK(label, target) if ((target) < nr_nodes) links.push_back(std::make_pair(std::string(label), size_t(target)))

    switch (node->nodetype) {
        case RESERVOIR: {
            Reservoir *r = dynamic_cast<Reservoir*>(node);
            if (node->outlet_tunnel_in_use)    ADD_LINK("OUTLET_TUNNEL", node->downstream_idnr_tunnel);
            if (node->outlet_hatch_in_use)     ADD_LINK("OUTLET_HATCH", node->downstream_idnr_hatch);
            if (node->outlet_overflow_in_use)  ADD_LINK((r && r->use_spillway) ? "SPILLWAY" : "OVERFLOW_CURVE", node->downstream_idnr_overflow);
            if (node->outlet_auto_qmin_in_use) ADD_LINK("OUTLET_AUTO_QMIN", node->downstream_idnr_auto_qmin);
            break;
        }
        case PSTATION:
            ADD_LINK("DOWNLINK_IDNR", node->downstream_idnr);
            break;
        case CHANNEL:
            if (node->downstream_node_in_use) ADD_LINK("DOWNSTREAM", node->downstream_idnr);
            break;
        case INTAKE: {
            Intake *in = dynamic_cast<Intake*>(node);
            if (in) {
                ADD_LINK("FLOODWATER_IDNR", in->floodwater_down_idnr);
                ADD_LINK("CAPCAP_DOWN_IDNR", in->capcap_down_idnr);
                ADD_LINK("HATCHREL_DOWN_IDNR", in->hatch_release_down_idnr);
            }
            break;
        }
        case PUMP: {
            Pump *p = dynamic_cast<Pump*>(node);
            if (p) {
                ADD_LINK("PUMP_SOURCE_IDNR", p->pump_source_idnr);
                ADD_LINK("PUMP_TARGET_IDNR", p->pump_target_idnr);
            }
            break;
        }
    }
    #undef ADD_LINK
    return links;
}

std::string TimestampString(Dataset *data, size_t t) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:00", data->year[t], data->month[t], data->day[t], data->hour[t]);
    return buf;
}

int WriteManifest(Herss *herss, const std::vector<NodeFileInfo> &infos) {
    GlobalConfig *gc = herss->gc;
    std::ofstream out(gc->manifest_file.c_str());
    if (!out.is_open()) {
        LOG_ERR("Cannot open manifest file " + gc->manifest_file);
        return -1;
    }

    out << "{\n";
    out << "  \"schema_version\": 1,\n";
    out << "  \"herss_version\": " << JsonString(VERSION) << ",\n";
    out << "  \"system_name\": " << JsonString(gc->systemname) << ",\n";

    out << "  \"time\": {\n";
    if (gc->stps > 0 && herss->data != NULL) {
        std::set<int> steps;
        for (size_t t = 0; t < gc->stps; t++) steps.insert(herss->getDeltaT(t));
        out << "    \"start\": " << JsonString(TimestampString(herss->data, 0)) << ",\n";
        out << "    \"end\": " << JsonString(TimestampString(herss->data, gc->stps - 1)) << ",\n";
        out << "    \"timestep_seconds\": [";
        bool first = true;
        for (std::set<int>::const_iterator it = steps.begin(); it != steps.end(); ++it) {
            out << (first ? "" : ", ") << *it;
            first = false;
        }
        out << "],\n";
    }
    out << "    \"steps\": " << gc->stps << "\n";
    out << "  },\n";

    out << "  \"inputs\": {\n";
    out << "    \"global\": " << JsonString(gc->globalfile) << ",\n";
    out << "    \"topology\": " << JsonString(gc->topologyfile) << ",\n";
    out << "    \"price\": " << JsonString(gc->pricefile) << ",\n";
    out << "    \"inflow\": " << JsonString(gc->inflowfile) << ",\n";
    out << "    \"actions\": " << JsonString(gc->actionsfile) << ",\n";
    out << "    \"start_state\": " << JsonString(gc->start_statefile) << "\n";
    out << "  },\n";

    out << "  \"nodes\": [\n";
    for (size_t n = 0; n < gc->nr_nodes; n++) {
        Node *node = herss->rs->nodes[n];
        const NodeFileInfo &info = infos[n];

        out << "    {\n";
        out << "      \"id\": " << node->idnr << ",\n";
        out << "      \"name\": " << JsonString(node->nodename) << ",\n";
        out << "      \"type\": " << JsonString(EnumToString(node->nodetype)) << ",\n";

        out << "      \"files\": {";
        bool first = true;
        if (info.has_txt) { out << "\"txt\": " << JsonString(info.txt_name); first = false; }
        if (info.has_csv) { out << (first ? "" : ", ") << "\"csv\": " << JsonString(info.csv_name); }
        out << "},\n";

        std::vector<std::pair<std::string, size_t> > links = CollectLinks(node, gc->nr_nodes);
        out << "      \"downstream\": [";
        for (size_t l = 0; l < links.size(); l++) {
            out << (l ? ", " : "") << "{\"via\": " << JsonString(links[l].first) << ", \"to\": " << links[l].second << "}";
        }
        out << "],\n";

        out << "      \"columns\": [";
        for (size_t c = 0; c < info.names.size(); c++) {
            out << (c ? ", " : "") << "{\"name\": " << JsonString(info.names[c])
                << ", \"unit\": " << JsonString(c < info.units.size() ? info.units[c] : "") << "}";
        }
        out << "]\n";
        out << "    }" << (n + 1 < gc->nr_nodes ? "," : "") << "\n";
    }
    out << "  ]\n";
    out << "}\n";
    return 0;
}

}  // namespace


int WriteOutputFormats(Herss *herss) {
    GlobalConfig *gc = herss->gc;
    const bool want_csv  = (gc->nodefile_format != "TXT");
    const bool keep_txt  = (gc->nodefile_format != "CSV");
    const bool want_json = !gc->manifest_file.empty();

    if (!want_csv && !want_json) return 0;

    if (want_csv && !gc->write_nodefiles) {
        LOG_WARN("NODEFILE_FORMAT " + gc->nodefile_format + " has no effect when WRITE_NODEFILES is 0.");
    }

    std::vector<NodeFileInfo> infos(gc->nr_nodes);

    if (gc->write_nodefiles) {
        for (size_t n = 0; n < gc->nr_nodes; n++) {
            Node *node = herss->rs->nodes[n];
            NodeFileInfo &info = infos[n];

            // Same name as the Node::WriteNodeOutput functions use.
            const std::string base = "node" + std::to_string(node->idnr) + "_" + node->nodename;
            info.txt_name = base + ".txt";
            info.csv_name = base + ".csv";
            const std::string txt_path = gc->outputdir + info.txt_name;

            Rows rows;
            if (!ReadNodeTxt(txt_path, info, rows)) {
                LOG_WARN("Could not read the column header of " + txt_path + ", node " + node->nodename + " is left out of the CSV and manifest columns.");
                info.names.clear();
                info.units.clear();
                info.has_txt = std::ifstream(txt_path.c_str()).good();
                continue;
            }
            info.has_txt = true;

            if (want_csv) {
                if (WriteNodeCsv(gc->outputdir + info.csv_name, info, rows)) {
                    info.has_csv = true;
                } else {
                    LOG_ERR("Cannot open file " + gc->outputdir + info.csv_name);
                }
                if (!keep_txt && info.has_csv) {
                    remove(txt_path.c_str());
                    info.has_txt = false;
                }
            }
        }
    }

    if (want_json) return WriteManifest(herss, infos);
    return 0;
}
