#!/usr/bin/env python3
"""
Pillar Configuration Validator
Validates all pillar config JSON files against the schema.
Exit codes: 0 = all valid, 1 = validation errors found
"""

import json
import sys
from pathlib import Path
from typing import Any


class SchemaValidator:
    """Simple JSON schema validator using stdlib only."""
    
    def __init__(self, schema: dict):
        self.schema = schema
    
    def validate(self, data: dict, schema: dict = None, path: str = "") -> list[str]:
        """Validate data against schema, return list of errors."""
        errors = []
        schema = schema or self.schema
        
        if schema.get("type") == "object":
            if not isinstance(data, dict):
                errors.append(f"{path or 'root'}: expected object, got {type(data).__name__}")
                return errors
            
            # Check required fields
            for req in schema.get("required", []):
                if req not in data:
                    errors.append(f"{path or 'root'}: missing required field '{req}'")
            
            # Validate properties
            props = schema.get("properties", {})
            for key, value in data.items():
                if key in props:
                    sub_path = f"{path}.{key}" if path else key
                    errors.extend(self.validate(value, props[key], sub_path))
            
            # Check minProperties
            min_props = schema.get("minProperties", 0)
            if len(data) < min_props:
                errors.append(f"{path or 'root'}: must have at least {min_props} properties")
            
            # Handle patternProperties
            pattern_props = schema.get("patternProperties", {})
            import re
            for key, value in data.items():
                for pattern, prop_schema in pattern_props.items():
                    if re.match(pattern, key):
                        sub_path = f"{path}.{key}" if path else key
                        errors.extend(self.validate(value, prop_schema, sub_path))
        
        elif schema.get("type") == "string":
            if not isinstance(data, str):
                errors.append(f"{path}: expected string, got {type(data).__name__}")
            elif "minLength" in schema and len(data) < schema["minLength"]:
                errors.append(f"{path}: string too short (min {schema['minLength']})")
        
        elif schema.get("type") == "integer":
            if not isinstance(data, int):
                errors.append(f"{path}: expected integer, got {type(data).__name__}")
            else:
                if "minimum" in schema and data < schema["minimum"]:
                    errors.append(f"{path}: value {data} below minimum {schema['minimum']}")
                if "maximum" in schema and data > schema["maximum"]:
                    errors.append(f"{path}: value {data} above maximum {schema['maximum']}")
        
        return errors


def validate_pillar_config(config: dict, schema: dict) -> list[str]:
    """Validate a single pillar config."""
    validator = SchemaValidator(schema)
    return validator.validate(config, schema)


def main():
    """Run validation on all pillar configs."""
    base_dir = Path(__file__).parent.parent  # Go up from scripts/ to root
    schema_path = base_dir / "schema" / "pillar_config_schema.json"
    
    # Load schema
    try:
        with open(schema_path, 'r', encoding='utf-8') as f:
            schema = json.load(f)
        print(f"[OK] Loaded schema: {schema_path}")
    except Exception as e:
        print(f"[ERR] Failed to load schema: {e}")
        sys.exit(1)
    
    # Find all pillar configs
    pillar_dirs = [d for d in base_dir.iterdir() 
                   if d.is_dir() and d.name.startswith("Pillar_")]
    
    if not pillar_dirs:
        print("✗ No pillar directories found!")
        sys.exit(1)
    
    print(f"Found {len(pillar_dirs)} pillar directories\n")
    
    all_errors = []
    pillar_indices = set()
    
    for pillar_dir in sorted(pillar_dirs):
        config_file = pillar_dir / f"{pillar_dir.name.split('_', 2)[2].lower()}_config.json"
        
        # Handle naming variations
        if not config_file.exists():
            # Try alternative naming
            for f in pillar_dir.glob("*_config.json"):
                config_file = f
                break
        
        if not config_file.exists():
            print(f"[ERR] {pillar_dir.name}: No config.json found")
            all_errors.append(f"{pillar_dir.name}: missing config file")
            continue
        
        try:
            with open(config_file, 'r', encoding='utf-8') as f:
                config = json.load(f)
        except json.JSONDecodeError as e:
            print(f"[ERR] {pillar_dir.name}: Invalid JSON - {e}")
            all_errors.append(f"{pillar_dir.name}: {e}")
            continue
        
        # Validate against schema
        errors = validate_pillar_config(config, schema)
        
        if errors:
            print(f"[ERR] {pillar_dir.name}: {len(errors)} validation errors")
            for err in errors[:3]:  # Show first 3 errors
                print(f"    - {err}")
            if len(errors) > 3:
                print(f"    ... and {len(errors) - 3} more")
            all_errors.extend([f"{pillar_dir.name}: {e}" for e in errors])
        else:
            # Check for index uniqueness
            idx = config.get("pillar_metadata", {}).get("index")
            if idx is not None:
                if idx in pillar_indices:
                    err = f"Duplicate pillar index: {idx}"
                    print(f"[ERR] {pillar_dir.name}: {err}")
                    all_errors.append(f"{pillar_dir.name}: {err}")
                else:
                    pillar_indices.add(idx)
            
            name = config.get("pillar_metadata", {}).get("name", "Unknown")
            print(f"[OK] {pillar_dir.name}: {name} (index {idx})")
    
    # Summary
    print("\n" + "="*60)
    if all_errors:
        print(f"VALIDATION FAILED: {len(all_errors)} errors found")
        sys.exit(1)
    else:
        print(f"VALIDATION PASSED: All {len(pillar_dirs)} pillar configs are valid")
        
        # Check for index gaps
        expected_indices = set(range(16))
        missing = expected_indices - pillar_indices
        if missing:
            print(f"[WARN] Missing pillar indices: {sorted(missing)}")
        else:
            print("[OK] All 16 pillar indices (0-15) present")
        
        sys.exit(0)


if __name__ == "__main__":
    main()
