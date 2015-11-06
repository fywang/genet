/**
 * Copyright (C) 2015 Felix Wang
 *
 * Simulation Tool for Asynchrnous Cortical Streams (stacs)
 *
 * ioyml.C
 * Handles YAML Ain't Markup Language format
 */

#include "genet.h"

// Using yaml-cpp (specification version 1.2)
#include "yaml-cpp/yaml.h"

/**************************************************************************
* Charm++ Read-Only Variables
**************************************************************************/
extern /*readonly*/ std::string filebase;
extern /*readonly*/ std::string filemod;
extern /*readonly*/ idx_t npdat;
extern /*readonly*/ idx_t npnet;
extern /*readonly*/ idx_t rngseed;


/**************************************************************************
* Main Configuration
**************************************************************************/

// Parse configuration file
//
int Main::ParseConfig(std::string configfile) {
  // Load config file
  CkPrintf("Loading config from %s\n", configfile.c_str());
  YAML::Node config;
  try {
    config = YAML::LoadFile(configfile);
  } catch (YAML::BadFile& e) {
    CkPrintf("  %s\n", e.what());
    return 1;
  }

  // Get configuration
  // Network data file
  try {
    filebase = config["filebase"].as<std::string>();
  } catch (YAML::RepresentationException& e) {
    CkPrintf("  filebase: %s\n", e.what());
    return 1;
  }
  if (mode == "build") {
    filemod = std::string("");
  }
  else {
    try {
      filemod = config["filemod"].as<std::string>();
    } catch (YAML::RepresentationException& e) {
      CkPrintf("  filemod not defined, defaulting to: \".o\"\n");
      filemod = std::string(".o");
    }
  }
  // Number of data files
  try {
    npdat = config["npdat"].as<idx_t>();
  } catch (YAML::RepresentationException& e) {
    CkPrintf("  npdat: %s\n", e.what());
    return 1;
  }
  // Number of network parts
  try {
    npnet = config["npnet"].as<idx_t>();
  } catch (YAML::RepresentationException& e) {
    CkPrintf("  npnet: %s\n", e.what());
    return 1;
  }
  // Random number seed
  try {
     rngseed = config["rngseed"].as<idx_t>();
  } catch (YAML::RepresentationException& e) {
    std::random_device rd;
    rngseed = rd();
    CkPrintf("  rngseed not defined, seeding with: %" PRIidx "\n", rngseed);
  }

  // Return success
  return 0;
}


/**************************************************************************
* Read in Models
**************************************************************************/

// Parse model file (multiple yaml docs in one file)
//
int Main::ReadModel() {
  // Load model file
  CkPrintf("Loading models from %s.model\n", filebase.c_str());
  YAML::Node modfile;
  try {
    modfile = YAML::LoadAllFromFile(filebase + ".model");
  } catch (YAML::BadFile& e) {
    CkPrintf("  %s\n", e.what());
    return 1;
  }

  // Setup model data
  models.resize(modfile.size());
  // Model map
  modmap.clear();
  modmap[std::string("none")] = 0;
  
  // Graph types
  graphtype.resize(GRAPHTYPE_NTYPE);
  graphtype[GRAPHTYPE_VTX] = std::string("vertex");
  graphtype[GRAPHTYPE_EDG] = std::string("edge");

  // Get model data
  for (idx_t i = 0; i < modfile.size(); ++i) {
    std::string type;

    try {
      // type
      type = modfile[i]["type"].as<std::string>();
    } catch (YAML::RepresentationException& e) {
      CkPrintf("  type: %s\n", e.what());
      return 1;
    }
    // Set type
    if (type == "vertex") {
      models[i].type = GRAPHTYPE_VTX;
    }
    else if (type == "edge") {
      models[i].type = GRAPHTYPE_EDG;
    }
    else {
      CkPrintf("  type: '%s' unknown type\n", type.c_str());
      return 1;
    }
    try {
      // modname
      models[i].modname = modfile[i]["modname"].as<std::string>();
      // modmap
      modmap[models[i].modname] = i+1;
    } catch (YAML::RepresentationException& e) {
      CkPrintf("  modname: %s\n", e.what());
      return 1;
    }

    // States are their own 'node'
    YAML::Node state = modfile[i]["state"];
    if (state.size() == 0) {
      CkPrintf("  warning: %s has no state\n", models[i].modname.c_str());
    }

    // preallocate space
    models[i].rngtype.resize(state.size());
    models[i].rngparam.resize(state.size());

    // loop through the states
    for (idx_t j = 0; j < state.size(); ++j) {
      std::string rngtype;
      try {
        // rngtype
        rngtype = state[j]["type"].as<std::string>();
      } catch (YAML::RepresentationException& e) {
        CkPrintf("  state type: %s\n", e.what());
        return 1;
      }
      // based on rng type, get params
      if (rngtype == "constant") {
        // Constant value
        models[i].rngtype[j] = RNGTYPE_CONST;
        models[i].rngparam[j].resize(RNGPARAM_CONST);
        try {
          // value
          models[i].rngparam[j][0] = state[j]["value"].as<real_t>();
        } catch (YAML::RepresentationException& e) {
          CkPrintf("  state constant value: %s\n", e.what());
          return 1;
        }
      }
      else if (rngtype == "uniform") {
        // Uniform distribution
        models[i].rngtype[j] = RNGTYPE_UNIF;
        models[i].rngparam[j].resize(RNGPARAM_UNIF);
        try {
          // min value
          models[i].rngparam[j][0] = state[j]["min"].as<real_t>();
        } catch (YAML::RepresentationException& e) {
          CkPrintf("  state uniform min: %s\n", e.what());
          return 1;
        }
        try {
          // max value
          models[i].rngparam[j][1] = state[j]["max"].as<real_t>();
        } catch (YAML::RepresentationException& e) {
          CkPrintf("  state uniform max: %s\n", e.what());
          return 1;
        }
      }
      else if (rngtype == "normal") {
        // Normal distribution
        models[i].rngtype[j] = RNGTYPE_NORM;
        models[i].rngparam[j].resize(RNGPARAM_NORM);
        try {
          // mean
          models[i].rngparam[j][0] = state[j]["mean"].as<real_t>();
        } catch (YAML::RepresentationException& e) {
          CkPrintf("  state normal mean: %s\n", e.what());
          return 1;
        }
        try {
          // standard deviation
          models[i].rngparam[j][1] = state[j]["std"].as<real_t>();
        } catch (YAML::RepresentationException& e) {
          CkPrintf("  state normal std: %s\n", e.what());
          return 1;
        }
      }
      else if (rngtype == "linear") {
        // Proportional to distance
        models[i].rngtype[j] = RNGTYPE_LIN;
        models[i].rngparam[j].resize(RNGPARAM_LIN);
        try {
          // scale
          models[i].rngparam[j][0] = state[j]["scale"].as<real_t>();
        } catch (YAML::RepresentationException& e) {
          CkPrintf("  state linear scale: %s\n", e.what());
          return 1;
        }
        try {
          // offset
          models[i].rngparam[j][1] = state[j]["offset"].as<real_t>();
        } catch (YAML::RepresentationException& e) {
          CkPrintf("  warning: state linear offset not defined,\n"
                   "           defaulting to 0\n");
          models[i].rngparam[j][1] = 0.0;
        }
      }
      else if (rngtype == "bounded linear") {
        // Proportional to distance
        models[i].rngtype[j] = RNGTYPE_BLIN;
        models[i].rngparam[j].resize(RNGPARAM_BLIN);
        try {
          // scale
          models[i].rngparam[j][0] = state[j]["scale"].as<real_t>();
        } catch (YAML::RepresentationException& e) {
          CkPrintf("  state bounded linear scale: %s\n", e.what());
          return 1;
        }
        try {
          // offset
          models[i].rngparam[j][1] = state[j]["offset"].as<real_t>();
        } catch (YAML::RepresentationException& e) {
          CkPrintf("  warning: state linear offset not defined,\n"
                   "           defaulting to 0\n");
          models[i].rngparam[j][1] = 0.0;
        }
        try {
          // min value
          models[i].rngparam[j][2] = state[j]["min"].as<real_t>();
        } catch (YAML::RepresentationException& e) {
          CkPrintf("  state bounded linear min: %s\n", e.what());
          return 1;
        }
        try {
          // max value
          models[i].rngparam[j][3] = state[j]["max"].as<real_t>();
        } catch (YAML::RepresentationException& e) {
          CkPrintf("  state bounded linear max: %s\n", e.what());
          return 1;
        }
      }
      else {
        CkPrintf("  error: '%s' unknown state type\n", rngtype.c_str());
        return 1;
      }
    }
  }

  // Return success
  return 0;
}


// Read in graph information
//
int Main::ReadGraph() {
  // Load model file
  CkPrintf("Loading graph from %s.graph\n", filebase.c_str());
  YAML::Node graphfile;
  try {
    graphfile = YAML::LoadFile(filebase + ".graph");
  } catch (YAML::BadFile& e) {
    CkPrintf("  %s\n", e.what());
    return 1;
  }

  // Vertices
  YAML::Node vertex = graphfile["vertex"];
  if (vertex.size() == 0) {
    CkPrintf("  error: graph has no vertices\n");
    return 1;
  }

  // preallocate space
  vertices.clear();
  vertices.resize(vertex.size());

  // loop through the vertices
  for (idx_t i = 0; i < vertex.size(); ++i) {
    std::string name;
    try {
      // modname
      name = vertex[i]["modname"].as<std::string>();
    } catch (YAML::RepresentationException& e) {
      CkPrintf("  vertex modname: %s\n", e.what());
      return 1;
    }
    if (modmap.find(name) == modmap.end()) {
      CkPrintf("  error: model %s not defined\n", name.c_str());
      return 1;
    }
    else {
      vertices[i].modidx = modmap[name];
    }
    try {
      // order
      vertices[i].order = vertex[i]["order"].as<idx_t>();
    } catch (YAML::RepresentationException& e) {
      CkPrintf("  vertex order: %s\n", e.what());
      return 1;
    }
    try {
      // shape
      name = vertex[i]["shape"].as<std::string>();
    } catch (YAML::RepresentationException& e) {
      CkPrintf("  vertex shape: %s\n", e.what());
      return 1;
    }
    if (name == "circle") {
      vertices[i].shape = VTXSHAPE_CIRCLE;
      vertices[i].param.resize(VTXPARAM_CIRCLE);
      try {
        // radius
        vertices[i].param[0] = vertex[i]["radius"].as<real_t>();
      } catch (YAML::RepresentationException& e) {
        CkPrintf("  vertex circle radius: %s\n", e.what());
        return 1;
      }
    }
    else if (name == "sphere") {
      vertices[i].shape = VTXSHAPE_SPHERE;
      vertices[i].param.resize(VTXPARAM_SPHERE);
      try {
        // radius
        vertices[i].param[0] = vertex[i]["radius"].as<real_t>();
      } catch (YAML::RepresentationException& e) {
        CkPrintf("  vertex circle radius: %s\n", e.what());
        return 1;
      }
    }
    else {
      CkPrintf("  error: '%s' unknown shape\n", name.c_str());
      return 1;
    }
  }

  // Edges
  YAML::Node edge = graphfile["edge"];
  if (edge.size() == 0) {
    CkPrintf("  error: graph has no edges\n");
    return 1;
  }

  // preallocate space
  edges.clear();
  edges.resize(edge.size());

  // loop through the edges
  for (idx_t i = 0; i < edges.size(); ++i) {
    std::string name;
    std::vector<std::string> names;
    try {
      // source
      name = edge[i]["source"].as<std::string>();
    } catch (YAML::RepresentationException& e) {
      CkPrintf("  edge source: %s\n", e.what());
      return 1;
    }
    if (modmap.find(name) == modmap.end()) {
      CkPrintf("  error: model %s not defined\n", name.c_str());
      return 1;
    }
    else {
      edges[i].source = modmap[name];
    }
    try {
      // target(s)
      names = edge[i]["target"].as<std::vector<std::string>>();
    } catch (YAML::RepresentationException& e) {
      CkPrintf("  edge targets: %s\n", e.what());
      return 1;
    }
    for (idx_t j = 0; j < names.size(); ++j) {
      if (modmap.find(names[j]) == modmap.end()) {
        CkPrintf("  error: model %s not defined\n", name.c_str());
        return 1;
      }
      else {
        edges[i].target.push_back(modmap[names[j]]);
      }
    }
    try {
      // modname
      name = edge[i]["modname"].as<std::string>();
    } catch (YAML::RepresentationException& e) {
      CkPrintf("  edge modname: %s\n", e.what());
      return 1;
    }
    if (modmap.find(name) == modmap.end()) {
      CkPrintf("  error: model %s not defined\n", name.c_str());
      return 1;
    }
    else {
      edges[i].modidx = modmap[name];
    }
    try {
      // cutoff
      edges[i].cutoff = edge[i]["cutoff"].as<real_t>();
    } catch (YAML::RepresentationException& e) {
      CkPrintf("  warning: cutoff not defined, defaulting to none\n");
      edges[i].cutoff = 0.0;
    }

    // Connection types are their own 'node'
    YAML::Node conn = edge[i]["connect"];
    if (conn.size() == 0) {
      CkPrintf("  error: edge %s has no connections\n", name.c_str());
    }

    // preallocate space
    edges[i].conntype.resize(conn.size());
    edges[i].connparam.resize(conn.size());

    // loop through the connections
    for (idx_t j = 0; j < conn.size(); ++j) {
      std::string conntype;
      try {
        // conntype
        conntype = conn[j]["type"].as<std::string>();
      } catch (YAML::RepresentationException& e) {
        CkPrintf("  connect type: %s\n", e.what());
        return 1;
      }
      // based on connection type, get params
      if (conntype == "uniform") {
        // Randomly connect
        edges[i].conntype[j] = CONNTYPE_UNIF;
        edges[i].connparam[j].resize(CONNPARAM_UNIF);
        try {
          // probability threshold
          edges[i].connparam[j][0] = conn[j]["prob"].as<real_t>();
        } catch (YAML::RepresentationException& e) {
          CkPrintf("  connect random prob: %s\n", e.what());
          return 1;
        }
      }
      else if (conntype == "sigmoid") {
        // Randomly connect
        edges[i].conntype[j] = CONNTYPE_SIG;
        edges[i].connparam[j].resize(CONNPARAM_SIG);
        try {
          // maximum probability
          edges[i].connparam[j][0] = conn[j]["maxprob"].as<real_t>();
        } catch (YAML::RepresentationException& e) {
          CkPrintf("  connect sigmoid maxprob: %s\n", e.what());
          return 1;
        }
        try {
          // sigmoid midpoint
          edges[i].connparam[j][1] = conn[j]["midpoint"].as<real_t>();
        } catch (YAML::RepresentationException& e) {
          CkPrintf("  connect sigmoid midpoint: %s\n", e.what());
          return 1;
        }
        try {
          // sigmoid slope
          edges[i].connparam[j][2] = conn[j]["slope"].as<real_t>();
        } catch (YAML::RepresentationException& e) {
          CkPrintf("  connect sigmoid slope: %s\n", e.what());
          return 1;
        }
      }
      else {
        CkPrintf("  error: '%s' unknown connection type\n", conntype.c_str());
        return 1;
      }
    }
  }

  // Check that only one type of edge may exist between any two given vertices
  // TODO: Enable multiple edges between vertices one day
  std::vector<std::vector<idx_t>> connections;
  connections.resize(models.size()+1);
  for (idx_t i = 0; i < edges.size(); ++i) {
    for (idx_t j = 0; j < edges[i].target.size(); ++j) {
      // add source target pairs
      connections[edges[i].source].push_back(edges[i].target[j]);
      // Sanity check that there are no 'none' sources or targets
      CkAssert(edges[i].source);
      CkAssert(edges[i].target[j]);
    }
  }
  // check for duplicates (connections[0] is 'none' model)
  for (idx_t i = 1; i < connections.size(); ++i) {
    std::sort(connections[i].begin(), connections[i].end());
    for (idx_t j = 1; j < connections[i].size(); ++j) {
      if (connections[i][j] == connections[i][j-1]) {
        CkPrintf("  error: multiple connection types between vertices\n"
                 "         %s to %s not allowed (yet)\n",
                 models[i-1].modname.c_str(), models[connections[i][j]-1].modname.c_str());
        return 1;
      }
    }
  }
  
  // Return success
  return 0;
}
