/*
 * Minimal single-header OBJ loader (subset of tinyobjloader API)
 * Drop-in replacement – supports positions, UVs, normals, faces (triangulated).
 *
 * Usage:
 *   #define TINYOBJLOADER_IMPLEMENTATION
 *   #include "tiny_obj_loader.h"
 *
 * Then call:
 *   tinyobj::attrib_t attrib;
 *   std::vector<tinyobj::shape_t>    shapes;
 *   std::vector<tinyobj::material_t> materials;
 *   std::string warn, err;
 *   bool ok = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, "model.obj");
 */

#pragma once
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>
#include <cstring>
#include <cstdlib>
#include <cassert>

namespace tinyobj {

/* ── Data structures ─────────────────────────────────────────────── */

struct attrib_t {
    std::vector<float> vertices;   // 3 floats per vertex  (x,y,z)
    std::vector<float> normals;    // 3 floats per normal  (nx,ny,nz)
    std::vector<float> texcoords;  // 2 floats per UV      (u,v)
};

struct index_t {
    int vertex_index;
    int normal_index;
    int texcoord_index;
    index_t() : vertex_index(-1), normal_index(-1), texcoord_index(-1) {}
};

struct mesh_t {
    std::vector<index_t>        indices;
    std::vector<unsigned char>  num_face_vertices; // 3 = triangle
    std::vector<int>            material_ids;
};

struct shape_t {
    std::string name;
    mesh_t      mesh;
};

struct material_t {
    std::string name;
    float diffuse[3];
    std::string diffuse_texname;
    material_t() { diffuse[0]=diffuse[1]=diffuse[2]=0.8f; }
};

/* ── Implementation ──────────────────────────────────────────────── */
#ifdef TINYOBJLOADER_IMPLEMENTATION

static inline void trim(std::string& s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    size_t e = s.find_last_not_of (" \t\r\n");
    s = (b==std::string::npos) ? "" : s.substr(b, e-b+1);
}

static index_t parseIndex(const std::string& tok) {
    index_t idx;
    // formats: v   v/vt   v//vn   v/vt/vn
    std::stringstream ss(tok);
    std::string part;
    int slot = 0;
    while (std::getline(ss, part, '/')) {
        if (!part.empty()) {
            int v = atoi(part.c_str());
            if (slot==0) idx.vertex_index   = v>0 ? v-1 : v;
            if (slot==1) idx.texcoord_index = v>0 ? v-1 : v;
            if (slot==2) idx.normal_index   = v>0 ? v-1 : v;
        }
        slot++;
    }
    return idx;
}

static bool LoadMtl(std::vector<material_t>& mats,
                    const std::string& filename,
                    const std::string& basedir)
{
    std::ifstream f((basedir+filename).c_str());
    if (!f) return false;
    material_t cur;
    bool hasMat = false;
    std::string line;
    while (std::getline(f, line)) {
        trim(line);
        if (line.empty() || line[0]=='#') continue;
        std::istringstream ss(line);
        std::string key; ss>>key;
        if (key=="newmtl") {
            if (hasMat) mats.push_back(cur);
            cur = material_t();
            ss >> cur.name;
            hasMat = true;
        } else if (key=="Kd") {
            ss >> cur.diffuse[0] >> cur.diffuse[1] >> cur.diffuse[2];
        } else if (key=="map_Kd") {
            ss >> cur.diffuse_texname;
        }
    }
    if (hasMat) mats.push_back(cur);
    return true;
}

static bool LoadObj(attrib_t* attrib,
                    std::vector<shape_t>* shapes,
                    std::vector<material_t>* materials,
                    std::string* warn,
                    std::string* err,
                    const char* filename,
                    const char* basedir_c = nullptr)
{
    std::string basedir = basedir_c ? std::string(basedir_c) : "";
    if (!basedir.empty() && basedir.back()!='/' && basedir.back()!='\\')
        basedir += '/';

    std::ifstream f((basedir+filename).c_str());
    if (!f) { if(err) *err="Cannot open "+std::string(filename); return false; }

    std::vector<float> tmp_v, tmp_vn, tmp_vt;
    shape_t cur; bool hasShape=false;
    std::string line;

    while (std::getline(f, line)) {
        trim(line);
        if (line.empty()||line[0]=='#') continue;
        std::istringstream ss(line);
        std::string key; ss>>key;

        if (key=="v") {
            float x,y,z; ss>>x>>y>>z;
            tmp_v.push_back(x); tmp_v.push_back(y); tmp_v.push_back(z);
        } else if (key=="vn") {
            float x,y,z; ss>>x>>y>>z;
            tmp_vn.push_back(x); tmp_vn.push_back(y); tmp_vn.push_back(z);
        } else if (key=="vt") {
            float u,v; ss>>u>>v;
            tmp_vt.push_back(u); tmp_vt.push_back(v);
        } else if (key=="o"||key=="g") {
            if (hasShape && !cur.mesh.indices.empty()) shapes->push_back(cur);
            cur = shape_t(); ss>>cur.name; hasShape=true;
        } else if (key=="f") {
            std::vector<index_t> face;
            std::string tok;
            while (ss>>tok) face.push_back(parseIndex(tok));
            // fan triangulate
            for (int i=1; i+1<(int)face.size(); i++) {
                cur.mesh.indices.push_back(face[0]);
                cur.mesh.indices.push_back(face[i]);
                cur.mesh.indices.push_back(face[i+1]);
                cur.mesh.num_face_vertices.push_back(3);
                cur.mesh.material_ids.push_back(-1);
            }
            hasShape = true;
        } else if (key=="mtllib") {
            std::string mtlfile; ss>>mtlfile;
            LoadMtl(*materials, mtlfile, basedir);
        }
    }
    if (hasShape && !cur.mesh.indices.empty()) shapes->push_back(cur);

    // flatten into attrib
    attrib->vertices  = tmp_v;
    attrib->normals   = tmp_vn;
    attrib->texcoords = tmp_vt;
    return true;
}

#endif // TINYOBJLOADER_IMPLEMENTATION
} // namespace tinyobj
