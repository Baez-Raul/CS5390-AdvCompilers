import json
import sys
from collections import deque

def form_blocks(body):
	blocks={}
	block = []
	label = None
	name_count = 0

	def label_block():
		nonlocal block, label, name_count
		if block:
			if label is None:
				label = f'B_{name_count}'
				name_count += 1
			blocks[label] = block
		block = []
		label = None

	for instr in body:
		if 'label' in instr:
			label_block()
			label = instr['label']
			block.append(instr)
		else:
			block.append(instr)

		if instr.get('op') in ('br','jmp','ret'):
			label_block()
	label_block()
	return blocks

def get_cfg(blocks):
	labels = list(blocks.keys())
	cfg = {lbl: [] for lbl in labels}

	for i, lbl in enumerate(labels):
		block = blocks[lbl]
		last = block[-1]

		if last.get('op') == 'jmp':
			cfg[lbl].append(last['labels'][0])
		elif last.get('op') == 'br':
			cfg[lbl].extend(last['labels'])
		elif last.get('op') == 'ret':
			pass
		else:
			if i + 1 < len(labels):
				cfg[lbl].append(labels[i+1])
	return cfg

def cfg_to_graph(cfg):
	out = ['cfg diagraph { ']
	for src, dests in cfg.items():
		for dst in dests:
			out.append(f' "{src}" -> "{dst}";')
	out.append('}')
	return "\n".join(out)

def get_path_lengths(cfg, entry):
	dist = {entry:0}
	queue = deque([entry])
	while queue:
		node = queue.popleft()
		for successor in cfg.get(node, []):
			if successor not in dist:
				dist[successor] = dist[node] + 1
				queue.append(successor)
	return dist

def reverse_postorder(cfg, entry):
	visited = set()
	order = []
	def dfs(node):
		if node in visited:
			return
		visited.add(node)
		for successor in cfg.get(node, []):
			dfs(successor)
		order.append(node)
	dfs(entry)
	return list(reversed(order))

def find_back_edges(cfg, entry):
    visited = set()
    stack = set()
    BEdges = []
    def dfs(u):
        visited.add(u)
        stack.add(u)
        for v in cfg.get(u, []):
            if v in stack:
                BEdges.append((u, v))
            elif v not in visited:
                dfs(v)
        stack.remove(u)
    dfs(entry)
    return BEdges

def is_reducible(cfg, entry):
    postorder = reverse_postorder(cfg, entry)
    post_index = {n: i for i, n in enumerate(postorder)}
    back_edges = find_back_edges(cfg, entry)
    for u, v in back_edges:
        if post_index[v] >= post_index[u]:
            return False
    return True

def mycfg():
	prog = json.load(sys.stdin)
	for func in prog['functions']:
		blocks = form_blocks(func['instrs'])
		cfg = get_cfg(blocks)

		print("CFG: ",cfg)
		entry = list(blocks.keys())[0]

		print("Path lengths:", get_path_lengths(cfg, entry))
		print("Reverse postorder:", reverse_postorder(cfg, entry))
		print("Back edges:", find_back_edges(cfg, entry))
		print("Reducible: ", is_reducible(cfg, entry))


if __name__ == '__main__':
	mycfg()
