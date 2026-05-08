#!/usr/bin/env python3
"""
RAG (Retrieval-Augmented Generation) System for Pillar AI.

Creates embeddings from pillar configs and identity files,
enables semantic search for relevant context retrieval.

Usage:
    python scripts/build_rag.py                    # Build vector store
    python scripts/build_rag.py --query "team conflict"  # Search
    python scripts/build_rag.py --interactive      # Interactive search
"""

import json
import os
import sys
import re
from pathlib import Path
from typing import List, Dict, Tuple

if sys.platform == 'win32':
    sys.stdout.reconfigure(encoding='utf-8')

# Add parent to path
sys.path.insert(0, str(Path(__file__).parent.parent))

try:
    import ollama
    HAS_OLLAMA = True
except ImportError:
    HAS_OLLAMA = False

# Chunk size for splitting documents
CHUNK_SIZE = 500
CHUNK_OVERLAP = 50


def chunk_text(text: str, chunk_size=CHUNK_SIZE, overlap=CHUNK_OVERLAP) -> List[str]:
    """Split text into overlapping chunks."""
    words = text.split()
    chunks = []
    i = 0

    while i < len(words):
        chunk = ' '.join(words[i:i + chunk_size])
        chunks.append(chunk)
        i += chunk_size - overlap

    return chunks if chunks else [text]


def extract_metadata_from_chunk(chunk: str, filepath: str) -> Dict:
    """Extract metadata from chunk content."""
    metadata = {
        'source': filepath,
        'pillar': None,
        'type': None,
        'has_citations': '[cite:' in chunk
    }

    # Determine pillar
    match = re.search(r'pillar_(\d+)_([A-Za-z]+)', filepath, re.IGNORECASE)
    if match:
        metadata['pillar'] = match.group(2).lower()
        metadata['pillar_index'] = int(match.group(1))

    # Determine type
    if 'config.json' in filepath:
        metadata['type'] = 'config'
    elif 'identity.md' in filepath:
        metadata['type'] = 'identity'
    elif 'interactions.md' in filepath:
        metadata['type'] = 'interactions'
    elif 'blackboard.md' in filepath:
        metadata['type'] = 'blackboard'

    return metadata


def build_vector_store(base_dir: str, model: str = "qwen2.5:0.5b"):
    """Build vector store from all pillar documents."""
    base_path = Path(base_dir)
    documents = []

    print("Scanning documents...")

    # Scan all relevant files
    for pattern in ['**/*_config.json', '**/*_identity.md', 'Interactions/*.md', 'colab_blackboard/*.md']:
        for filepath in base_path.glob(pattern):
            if 'schema' in str(filepath) or 'scripts' in str(filepath) or 'src' in str(filepath):
                continue

            try:
                with open(filepath, 'r', encoding='utf-8') as f:
                    content = f.read()

                # For JSON files, extract relevant text
                if filepath.suffix == '.json':
                    data = json.loads(content)
                    text_parts = []

                    # Extract all text fields
                    def extract_text(obj, prefix=""):
                        if isinstance(obj, dict):
                            for k, v in obj.items():
                                extract_text(v, f"{prefix}.{k}" if prefix else k)
                        elif isinstance(obj, list):
                            for i, item in enumerate(obj):
                                extract_text(item, f"{prefix}[{i}]")
                        elif isinstance(obj, str):
                            text_parts.append(obj)

                    extract_text(data)
                    content = '\n'.join(text_parts)

                # Chunk the content
                chunks = chunk_text(content)

                for i, chunk in enumerate(chunks):
                    metadata = extract_metadata_from_chunk(chunk, str(filepath.relative_to(base_path)))
                    metadata['chunk_index'] = i

                    doc = {
                        'id': f"{filepath.stem}_{i}",
                        'text': chunk,
                        'metadata': metadata
                    }

                    # Generate embedding if ollama available
                    if HAS_OLLAMA:
                        try:
                            response = ollama.embeddings(model=model, prompt=chunk)
                            doc['embedding'] = response['embedding']
                        except Exception as e:
                            print(f"[WARN] Embedding failed for {filepath.name}: {e}")
                            doc['embedding'] = None

                    documents.append(doc)

            except Exception as e:
                print(f"[WARN] Error processing {filepath}: {e}")

    # Save vector store
    output_path = base_path / 'rag_store.json'

    # Convert embeddings to lists if needed
    for doc in documents:
        if doc.get('embedding') is not None and hasattr(doc['embedding'], 'tolist'):
            doc['embedding'] = doc['embedding'].tolist()

    with open(output_path, 'w', encoding='utf-8') as f:
        json.dump({
            'documents': documents,
            'model': model if HAS_OLLAMA else None,
            'total_docs': len(documents)
        }, f, indent=2, ensure_ascii=False)

    print(f"[OK] Built vector store: {len(documents)} chunks -> {output_path}")
    return output_path


def cosine_similarity(a, b):
    """Calculate cosine similarity between two vectors."""
    dot = sum(x * y for x, y in zip(a, b))
    mag_a = sum(x * x for x in a) ** 0.5
    mag_b = sum(y * y for y in b) ** 0.5
    if mag_a == 0 or mag_b == 0:
        return 0
    return dot / (mag_a * mag_b)


def search_rag(query: str, base_dir: str, top_k: int = 5, model: str = "qwen2.5:0.5b"):
    """Search the vector store for relevant documents."""
    store_path = Path(base_dir) / 'rag_store.json'

    if not store_path.exists():
        print("[ERR] RAG store not found. Run with --build first.")
        return []

    with open(store_path, 'r', encoding='utf-8') as f:
        store = json.load(f)

    documents = store['documents']

    # If ollama available, use embedding-based search
    if HAS_OLLAMA and store.get('model'):
        try:
            response = ollama.embeddings(model=store['model'], prompt=query)
            query_embedding = response['embedding']

            # Score all documents
            scores = []
            for doc in documents:
                if doc.get('embedding'):
                    sim = cosine_similarity(query_embedding, doc['embedding'])
                    scores.append((sim, doc))

            # Sort by similarity
            scores.sort(key=lambda x: x[0], reverse=True)
            return [(score, doc) for score, doc in scores[:top_k]]

        except Exception as e:
            print(f"[WARN] Embedding search failed: {e}")

    # Fallback: TF-IDF-like keyword search
    query_words = set(query.lower().split())
    scores = []

    for doc in documents:
        doc_words = set(doc['text'].lower().split())
        overlap = len(query_words & doc_words)
        scores.append((overlap, doc))

    scores.sort(key=lambda x: x[0], reverse=True)
    return [(score, doc) for score, doc in scores[:top_k] if score > 0]


def print_search_results(results: List[Tuple[float, Dict]]):
    """Print search results."""
    print("=" * 60)
    print("RAG SEARCH RESULTS")
    print("=" * 60)

    for i, (score, doc) in enumerate(results, 1):
        metadata = doc['metadata']
        print(f"\n{i}. Score: {score:.3f}")
        print(f"   Source: {metadata['source']}")
        print(f"   Type: {metadata['type']} | Pillar: {metadata.get('pillar', 'N/A')}")
        print(f"   Citations: {'Yes' if metadata['has_citations'] else 'No'}")
        print(f"   Text: {doc['text'][:150]}...")


def interactive_search(base_dir: str):
    """Interactive RAG search loop."""
    print("RAG Interactive Search (type 'quit' to exit)")
    print("=" * 60)

    while True:
        try:
            query = input("\nQuery: ").strip()
            if query.lower() in ('quit', 'exit', 'q'):
                break
            if not query:
                continue

            results = search_rag(query, base_dir)
            print_search_results(results)

        except KeyboardInterrupt:
            print("\nExiting...")
            break


def main():
    import argparse

    parser = argparse.ArgumentParser(description="Pillar AI RAG System")
    parser.add_argument('--build', action='store_true', help='Build vector store')
    parser.add_argument('--query', help='Search query')
    parser.add_argument('--interactive', action='store_true', help='Interactive search')
    parser.add_argument('--model', default='qwen2.5:0.5b', help='Embedding model')
    parser.add_argument('--top-k', type=int, default=5, help='Number of results')

    args = parser.parse_args()

    base_dir = str(Path(__file__).parent.parent)

    if args.build:
        build_vector_store(base_dir, args.model)
    elif args.query:
        results = search_rag(args.query, base_dir, args.top_k)
        print_search_results(results)
    elif args.interactive:
        interactive_search(base_dir)
    else:
        print("Usage: --build | --query 'text' | --interactive")


if __name__ == "__main__":
    main()
