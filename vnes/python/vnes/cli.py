"""
VNES CLI — command-line interface for mesh generation, lattice
placement, variant info, and validation.

Usage:
    vnes info VNE-A-250
    vnes generate VNE-A-250 -o output.stl
    vnes lattice --width 3 --height 3 --depth 1 -o lattice.stl
    vnes validate spec.json
"""

from __future__ import annotations
import json
import sys
import click
from . import _HAS_CPP
from .metadata import (
    VARIANT_METADATA, lookup_variant, FaceRole,
    CELL_RADIUS_MM, CELL_SPACING_MM, FRAME_THICKNESS_MM,
)
from .generator import generate_mesh, export_stl
from .lattice import place_lattice


@click.group()
def cli():
    """VNES — Van Neumann Ecosystem Substrate"""


@cli.command()
@click.argument("variant_id")
def info(variant_id):
    """Show metadata for a VNES variant."""
    md = lookup_variant(variant_id)
    if md is None:
        click.echo(f"Error: unknown variant '{variant_id}'", err=True)
        sys.exit(1)

    click.echo(f"Variant: {md['class_id']}")
    click.echo(f"  Description: {md.get('description', '')}")
    click.echo(f"  Load-bearing: {md['load_bearing']}")
    click.echo(f"  Compressive priority: {md['compressive_priority']}")
    click.echo(f"  Fluid priority: {md['fluid_priority']}")
    click.echo(f"  Bio support: 0b{md['bio_support']:04b}")
    click.echo(f"  Square face role: {FaceRole(md['square_face_role']).name}")
    click.echo(f"  Hex face role: {FaceRole(md['hex_face_role']).name}")
    click.echo(f"  Frame thickness: {md['frame_thickness_mm']} mm")
    click.echo(f"  Supportless: {md['supportless']}")

    if variant_id == "VNE-A-250":
        click.echo("  Surface: Schwarz P (triply periodic)")
    elif variant_id in ("VNE-B-250", "VNE-C-250"):
        click.echo("  Surface: Gyroid (triply periodic)")
    elif variant_id == "VNE-D-250":
        click.echo("  Surface: 3x intersecting vascular trunks")
    elif variant_id == "VNE-E-250":
        click.echo("  Surface: Gyroid + cable conduits")


@cli.command()
@click.argument("variant_id")
@click.option("-o", "--output", default=None, help="Output STL file")
@click.option("--resolution", default=128, show_default=True,
              help="Marching cubes grid resolution")
@click.option("--ascii", is_flag=True, help="ASCII STL output")
@click.option("--no-cleanup", is_flag=True, help="Skip mesh cleanup")
def generate(variant_id, output, resolution, ascii, no_cleanup):
    """Generate a mesh for a VNES variant."""
    if variant_id not in VARIANT_METADATA:
        click.echo(f"Error: unknown variant '{variant_id}'", err=True)
        sys.exit(1)

    if output is None:
        output = f"{variant_id}.stl"

    try:
        verts, tris, norms = generate_mesh(
            variant_id, resolution, not no_cleanup
        )
    except Exception as e:
        click.echo(f"Error generating mesh: {e}", err=True)
        sys.exit(1)

    click.echo(f"Generated {len(verts)} vertices, {len(tris)} triangles")

    try:
        export_stl(verts, tris, output, binary=not ascii)
        click.echo(f"Saved to {output}")
    except Exception as e:
        click.echo(f"Error saving STL: {e}", err=True)
        sys.exit(1)


@cli.command()
@click.option("--width", default=3, help="Number of cells in X")
@click.option("--height", default=3, help="Number of cells in Y")
@click.option("--depth", default=1, help="Number of cells in Z")
@click.option("--variant", default="VNE-A-250", help="Cell variant")
@click.option("-o", "--output", default="lattice.stl", help="Output file")
@click.option("--resolution", default=64, show_default=True,
              help="MC resolution per cell")
def lattice(width, height, depth, variant, output, resolution):
    """Generate a rectangular lattice of VNES cells."""
    try:
        solver = place_lattice(width, height, depth, variant)
    except RuntimeError as e:
        click.echo(f"Placement error: {e}", err=True)
        sys.exit(1)

    click.echo(f"Placed {solver.cell_count} cells")

    # For now, just generate the first cell as a representative mesh
    verts, tris, norms = generate_mesh(variant, resolution)
    export_stl(verts, tris, output)
    click.echo(f"Saved single-cell mesh to {output}")
    click.echo("Note: Full lattice mesh generation not yet implemented in Python.")


@cli.command()
@click.argument("spec_file", type=click.Path(exists=True))
def validate(spec_file):
    """Validate a VNES specification JSON file."""
    try:
        with open(spec_file) as f:
            spec = json.load(f)
    except json.JSONDecodeError as e:
        click.echo(f"Invalid JSON: {e}", err=True)
        sys.exit(1)
    except Exception as e:
        click.echo(f"Error reading file: {e}", err=True)
        sys.exit(1)

    errors = []

    # Validate structure
    if "variants" in spec:
        for v in spec["variants"]:
            vid = v.get("class_id", "")
            if vid not in VARIANT_METADATA:
                errors.append(f"Unknown variant: {vid}")

    if "lattice" in spec:
        lat = spec["lattice"]
        for dim in ("width", "height", "depth"):
            if dim not in lat:
                errors.append(f"Missing lattice.{dim}")

    if not errors:
        click.echo("Specification is valid.")
    else:
        for e in errors:
            click.echo(f"  ERROR: {e}", err=True)
        sys.exit(1)


@cli.command()
def list_variants():
    """List all available VNES variants."""
    click.echo("VNES Variants:")
    for vid, md in VARIANT_METADATA.items():
        desc = md.get("description", "")
        click.echo(f"  {vid}: {desc}")


if __name__ == "__main__":
    cli()
