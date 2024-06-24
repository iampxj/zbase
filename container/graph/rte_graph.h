/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#ifndef _RTE_GRAPH_H_
#define _RTE_GRAPH_H_

/**
 * @file rte_graph.h
 *
 * Graph architecture abstracts the data processing functions as
 * "node" and "link" them together to create a complex "graph" to enable
 * reusable/modular data processing functions.
 *
 * This API enables graph framework operations such as create, lookup,
 * dump and destroy on graph and node operations such as clone,
 * edge update, and edge shrink, etc. The API also allows to create the stats
 * cluster to monitor per graph and per node stats.
 */
#include "basework/generic.h"
#include "basework/lib/printer.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RTE_GRAPH_NAMESIZE 32 /**< Max length of graph name. */
#define RTE_NODE_NAMESIZE 32  /**< Max length of node name. */
#define RTE_GRAPH_PCAP_FILE_SZ 32 /**< Max length of pcap file name. */
#define RTE_GRAPH_OFF_INVALID UINT32_MAX /**< Invalid graph offset. */
#define RTE_NODE_ID_INVALID UINT32_MAX   /**< Invalid node id. */
#define RTE_EDGE_ID_INVALID UINT16_MAX   /**< Invalid edge id. */
#define RTE_GRAPH_ID_INVALID UINT16_MAX  /**< Invalid graph id. */
#define RTE_GRAPH_FENCE 0xdeadbeef12345678ULL /**< Graph fence data. */

typedef uint32_t rte_graph_off_t;  /**< Graph offset type. */
typedef uint32_t rte_node_t;       /**< Node id type. */
typedef uint16_t rte_edge_t;       /**< Edge id type. */
typedef uint16_t rte_graph_t;      /**< Graph id type. */

#ifndef RTE_GRAPH_BURST_SIZE
#define RTE_GRAPH_BURST_SIZE 2
#endif

/** Burst size in terms of log2 */
#if RTE_GRAPH_BURST_SIZE == 1
#define RTE_GRAPH_BURST_SIZE_LOG2 0  /**< Object burst size of 1. */
#elif RTE_GRAPH_BURST_SIZE == 2
#define RTE_GRAPH_BURST_SIZE_LOG2 1  /**< Object burst size of 2. */
#elif RTE_GRAPH_BURST_SIZE == 4
#define RTE_GRAPH_BURST_SIZE_LOG2 2  /**< Object burst size of 4. */
#elif RTE_GRAPH_BURST_SIZE == 8
#define RTE_GRAPH_BURST_SIZE_LOG2 3  /**< Object burst size of 8. */
#elif RTE_GRAPH_BURST_SIZE == 16
#define RTE_GRAPH_BURST_SIZE_LOG2 4  /**< Object burst size of 16. */
#elif RTE_GRAPH_BURST_SIZE == 32
#define RTE_GRAPH_BURST_SIZE_LOG2 5  /**< Object burst size of 32. */
#elif RTE_GRAPH_BURST_SIZE == 64
#define RTE_GRAPH_BURST_SIZE_LOG2 6  /**< Object burst size of 64. */
#elif RTE_GRAPH_BURST_SIZE == 128
#define RTE_GRAPH_BURST_SIZE_LOG2 7  /**< Object burst size of 128. */
#elif RTE_GRAPH_BURST_SIZE == 256
#define RTE_GRAPH_BURST_SIZE_LOG2 8  /**< Object burst size of 256. */
#else
#error "Unsupported burst size"
#endif

/* Forward declaration */
struct rte_node;  /**< Node object */
struct rte_graph; /**< Graph object */

/**
 * Node process function.
 *
 * The function invoked when the worker thread walks on nodes using
 * rte_graph_walk().
 *
 * @param graph
 *   Pointer to the graph object.
 * @param node
 *   Pointer to the node object.
 * @param objs
 *   Pointer to an array of objects to be processed.
 * @param nb_objs
 *   Number of objects in the array.
 *
 * @return
 *   Number of objects processed.
 *
 * @see rte_graph_walk()
 */
typedef uint16_t (*rte_node_process_t)(struct rte_graph *graph,
				       struct rte_node *node, void **objs,
				       uint16_t nb_objs);

/**
 * Node initialization function.
 *
 * The function invoked when the user creates the graph using rte_graph_create()
 *
 * @param graph
 *   Pointer to the graph object.
 * @param node
 *   Pointer to the node object.
 *
 * @return
 *   - 0: Success.
 *   -<0: Failure.
 *
 * @see rte_graph_create()
 */
typedef int (*rte_node_init_t)(const struct rte_graph *graph,
			       struct rte_node *node);

/**
 * Node finalization function.
 *
 * The function invoked when the user destroys the graph using
 * rte_graph_destroy().
 *
 * @param graph
 *   Pointer to the graph object.
 * @param node
 *   Pointer to the node object.
 *
 * @see rte_graph_destroy()
 */
typedef void (*rte_node_fini_t)(const struct rte_graph *graph,
				struct rte_node *node);

/**
 * Structure to hold configuration parameters for creating the graph.
 *
 * @see rte_graph_create()
 */
struct rte_graph_param {
	uint16_t nb_node_patterns;  /**< Number of node patterns. */
	const char **node_patterns;
};

/**
 * Create Graph.
 *
 * Create memory reel, detect loops and find isolated nodes.
 *
 * @param name
 *   Unique name for this graph.
 * @param prm
 *   Graph parameter, includes node names and count to be included
 *   in this graph.
 *
 * @return
 *   Unique graph id on success, RTE_GRAPH_ID_INVALID otherwise.
 */
rte_graph_t rte_graph_create(const char *name, struct rte_graph_param *prm);

/**
 * Destroy Graph.
 *
 * Free Graph memory reel.
 *
 * @param id
 *   id of the graph to destroy.
 *
 * @return
 *   0 on success, error otherwise.
 */
int rte_graph_destroy(rte_graph_t id);

/**
 * Clone Graph.
 *
 * Clone a graph from static graph (graph created from rte_graph_create()). And
 * all cloned graphs attached to the parent graph MUST be destroyed together
 * for fast schedule design limitation (stop ALL graph walk firstly).
 *
 * @param id
 *   Static graph id to clone from.
 * @param name
 *   Name of the new graph. The library prepends the parent graph name to the
 * user-specified name. The final graph name will be,
 * "parent graph name" + "-" + name.
 * @param prm
 *   Graph parameter, includes model-specific parameters in this graph.
 *
 * @return
 *   Valid graph id on success, RTE_GRAPH_ID_INVALID otherwise.
 */
rte_graph_t rte_graph_clone(rte_graph_t id, const char *name, struct rte_graph_param *prm);

/**
 * Get graph id from graph name.
 *
 * @param name
 *   Name of the graph to get id.
 *
 * @return
 *   Graph id on success, RTE_GRAPH_ID_INVALID otherwise.
 */
rte_graph_t rte_graph_from_name(const char *name);

/**
 * Get graph name from graph id.
 *
 * @param id
 *   id of the graph to get name.
 *
 * @return
 *   Graph name on success, NULL otherwise.
 */
char *rte_graph_id_to_name(rte_graph_t id);

/**
 * Export the graph as graph viz dot file
 *
 * @param name
 *   Name of the graph to export.
 * @param f
 *   File pointer to export the graph.
 *
 * @return
 *   0 on success, error otherwise.
 */
int rte_graph_export(const char *name, struct printer *pr);

/**
 * Get graph object from its name.
 *
 * Typical usage of this API to get graph objects in the worker thread and
 * followed calling rte_graph_walk() in a loop.
 *
 * @param name
 *   Name of the graph.
 *
 * @return
 *   Graph pointer on success, NULL otherwise.
 *
 * @see rte_graph_walk()
 */
struct rte_graph *rte_graph_lookup(const char *name);

/**
 * Get maximum number of graph available.
 *
 * @return
 *   Maximum graph count.
 */
rte_graph_t rte_graph_max_count(void);

/**
 * Dump the graph information to file.
 *
 * @param f
 *   File pointer to dump graph info.
 * @param id
 *   Graph id to get graph info.
 */
void rte_graph_dump(struct printer *pr, rte_graph_t id);

/**
 * Dump all graphs information to file
 *
 * @param f
 *   File pointer to dump graph info.
 */
void rte_graph_list_dump(struct printer *pr);

/**
 * Dump graph information along with node info to file
 *
 * @param f
 *   File pointer to dump graph info.
 * @param graph
 *   Graph pointer to get graph info.
 * @param all
 *   true to dump nodes in the graph.
 */
void rte_graph_obj_dump(struct printer *pr, struct rte_graph *graph, bool all);

/** Macro to browse rte_node object after the graph creation */
#define rte_graph_foreach_node(count, off, graph, node)                        \
	for (count = 0, off = graph->nodes_start,                              \
	     node = RTE_PTR_ADD(graph, off);                                   \
	     count < graph->nb_nodes;                                          \
	     off = node->next, node = RTE_PTR_ADD(graph, off), count++)

/**
 * Get node object with in graph from id.
 *
 * @param graph_id
 *   Graph id to get node pointer from.
 * @param node_id
 *   Node id to get node pointer.
 *
 * @return
 *   Node pointer on success, NULL otherwise.
 */
struct rte_node *rte_graph_node_get(rte_graph_t graph_id, rte_node_t node_id);

/**
 * Get node pointer with in graph from name.
 *
 * @param graph
 *   Graph name to get node pointer from.
 * @param name
 *   Node name to get the node pointer.
 *
 * @return
 *   Node pointer on success, NULL otherwise.
 */
struct rte_node *rte_graph_node_get_by_name(const char *graph,
					    const char *name);

/**
 * Structure defines the node registration parameters.
 *
 * @see __rte_node_register(), RTE_NODE_REGISTER()
 */
struct rte_node_register {
	char name[RTE_NODE_NAMESIZE]; /**< Name of the node. */
	uint64_t flags;		      /**< Node configuration flag. */
#define RTE_NODE_SOURCE_F (1ULL << 0) /**< Node type is source. */
	rte_node_process_t process; /**< Node process function. */
	rte_node_init_t init;       /**< Node init function. */
	rte_node_fini_t fini;       /**< Node fini function. */
	rte_node_t id;		    /**< Node Identifier. */
	rte_node_t parent_id;       /**< Identifier of parent node. */
	rte_edge_t nb_edges;        /**< Number of edges from this node. */
	const char *next_nodes[];   /**< Names of next nodes. */
};

/**
 * Register new packet processing node. Nodes can be registered
 * dynamically via this call or statically via the RTE_NODE_REGISTER
 * macro.
 *
 * @param node
 *   Valid node pointer with name, process function and next_nodes.
 *
 * @return
 *   Valid node id on success, RTE_NODE_ID_INVALID otherwise.
 *
 * @see RTE_NODE_REGISTER()
 */
rte_node_t __rte_node_register(const struct rte_node_register *node);

/**
 * Register a static node.
 *
 * The static node is registered through the constructor scheme, thereby, it can
 * be used in a multi-process scenario.
 *
 * @param node
 *   Valid node pointer with name, process function, and next_nodes.
 */
#define RTE_NODE_REGISTER(node)  \
	rte_sysinit(rte_node_register_##node, 200) { \
		node.parent_id = RTE_NODE_ID_INVALID;    \
		node.id = __rte_node_register(&node);    \
	}

/**
 * Clone a node from static node(node created from RTE_NODE_REGISTER).
 *
 * @param id
 *   Static node id to clone from.
 * @param name
 *   Name of the new node. The library prepends the parent node name to the
 * user-specified name. The final node name will be,
 * "parent node name" + "-" + name.
 *
 * @return
 *   Valid node id on success, RTE_NODE_ID_INVALID otherwise.
 */
rte_node_t rte_node_clone(rte_node_t id, const char *name);

/**
 * Get node id from node name.
 *
 * @param name
 *   Valid node name. In the case of the cloned node, the name will be
 * "parent node name" + "-" + name.
 *
 * @return
 *   Valid node id on success, RTE_NODE_ID_INVALID otherwise.
 */
rte_node_t rte_node_from_name(const char *name);

/**
 * Get node name from node id.
 *
 * @param id
 *   Valid node id.
 *
 * @return
 *   Valid node name on success, NULL otherwise.
 */
char *rte_node_id_to_name(rte_node_t id);

/**
 * Get the number of edges(next-nodes) for a node from node id.
 *
 * @param id
 *   Valid node id.
 *
 * @return
 *   Valid edge count on success, RTE_EDGE_ID_INVALID otherwise.
 */
rte_edge_t rte_node_edge_count(rte_node_t id);

/**
 * Update the edges for a node from node id.
 *
 * @param id
 *   Valid node id.
 * @param from
 *   Index to update the edges from. RTE_EDGE_ID_INVALID is valid,
 * in that case, it will be added to the end of the list.
 * @param next_nodes
 *   Name of the edges to update.
 * @param nb_edges
 *   Number of edges to update.
 *
 * @return
 *   Valid edge count on success, 0 otherwise.
 */
rte_edge_t rte_node_edge_update(rte_node_t id, rte_edge_t from,
				const char **next_nodes, uint16_t nb_edges);

/**
 * Shrink the edges to a given size.
 *
 * @param id
 *   Valid node id.
 * @param size
 *   New size to shrink the edges.
 *
 * @return
 *   New size on success, RTE_EDGE_ID_INVALID otherwise.
 */
rte_edge_t rte_node_edge_shrink(rte_node_t id, rte_edge_t size);

/**
 * Get the edge names from a given node.
 *
 * @param id
 *   Valid node id.
 * @param[out] next_nodes
 *   Buffer to copy the edge names. The NULL value is allowed in that case,
 * the function returns the size of the array that needs to be allocated.
 *
 * @return
 *   When next_nodes == NULL, it returns the size of the array else
 *  number of item copied.
 */
rte_node_t rte_node_edge_get(rte_node_t id, char *next_nodes[]);

/**
 * Get maximum nodes available.
 *
 * @return
 *   Maximum nodes count.
 */
rte_node_t rte_node_max_count(void);

/**
 * Dump node info to file.
 *
 * @param f
 *   File pointer to dump the node info.
 * @param id
 *   Node id to get the info.
 */
void rte_node_dump(struct printer *pr, rte_node_t id);

/**
 * Dump all node info to file.
 *
 * @param f
 *   File pointer to dump the node info.
 */
void rte_node_list_dump(struct printer *pr);

/**
 * Test the validity of node id.
 *
 * @param id
 *   Node id to check.
 *
 * @return
 *   1 if valid id, 0 otherwise.
 */
static __rte_always_inline int
rte_node_is_invalid(rte_node_t id)
{
	return (id == RTE_NODE_ID_INVALID);
}

/**
 * Test the validity of edge id.
 *
 * @param id
 *   Edge node id to check.
 *
 * @return
 *   1 if valid id, 0 otherwise.
 */
static __rte_always_inline int
rte_edge_is_invalid(rte_edge_t id)
{
	return (id == RTE_EDGE_ID_INVALID);
}

/**
 * Test the validity of graph id.
 *
 * @param id
 *   Graph id to check.
 *
 * @return
 *   1 if valid id, 0 otherwise.
 */
static __rte_always_inline int
rte_graph_is_invalid(rte_graph_t id)
{
	return (id == RTE_GRAPH_ID_INVALID);
}

/**
 * Test stats feature support.
 *
 * @return
 *   1 if stats enabled, 0 otherwise.
 */
static __rte_always_inline int
rte_graph_has_stats_feature(void)
{
#ifdef RTE_LIBRTE_GRAPH_STATS
	return RTE_LIBRTE_GRAPH_STATS;
#else
	return 0;
#endif
}

#ifdef __cplusplus
}
#endif

#endif /* _RTE_GRAPH_H_ */
