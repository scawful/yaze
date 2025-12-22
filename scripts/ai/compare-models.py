#!/usr/bin/env python3
"""
YAZE AI Model Comparison Report Generator

Generates comparison reports from evaluation results.

Usage:
    python compare-models.py results/eval-*.json
    python compare-models.py --format markdown results/eval-20241125.json
    python compare-models.py --best results/eval-*.json
"""

import argparse
import json
import os
import sys
from datetime import datetime
from pathlib import Path
from typing import Any


def load_results(file_paths: list[str]) -> list[dict]:
    """Load evaluation results from JSON files."""
    results = []
    for path in file_paths:
        try:
            with open(path, 'r') as f:
                data = json.load(f)
                data['_source_file'] = path
                results.append(data)
        except Exception as e:
            print(f"Warning: Could not load {path}: {e}", file=sys.stderr)
    return results


def merge_results(results: list[dict]) -> dict:
    """Merge multiple result files into a single comparison."""
    merged = {
        "sources": [],
        "models": {},
        "timestamp": datetime.now().isoformat()
    }
    
    for result in results:
        merged["sources"].append(result.get('_source_file', 'unknown'))
        
        for model, model_data in result.get('models', {}).items():
            if model not in merged["models"]:
                merged["models"][model] = {
                    "runs": [],
                    "summary": {}
                }
            
            merged["models"][model]["runs"].append({
                "source": result.get('_source_file'),
                "timestamp": result.get('timestamp'),
                "summary": model_data.get('summary', {}),
                "task_count": len(model_data.get('tasks', []))
            })
    
    # Calculate averages across runs
    for model, data in merged["models"].items():
        runs = data["runs"]
        if runs:
            data["summary"] = {
                "avg_accuracy": sum(r["summary"].get("avg_accuracy", 0) for r in runs) / len(runs),
                "avg_completeness": sum(r["summary"].get("avg_completeness", 0) for r in runs) / len(runs),
                "avg_tool_usage": sum(r["summary"].get("avg_tool_usage", 0) for r in runs) / len(runs),
                "avg_response_time": sum(r["summary"].get("avg_response_time", 0) for r in runs) / len(runs),
                "overall_score": sum(r["summary"].get("overall_score", 0) for r in runs) / len(runs),
                "run_count": len(runs)
            }
    
    return merged


def format_table(merged: dict) -> str:
    """Format results as ASCII table."""
    lines = []
    
    lines.append("┌" + "─"*78 + "┐")
    lines.append("│" + " "*18 + "YAZE AI Model Comparison Report" + " "*27 + "│")
    lines.append("│" + " "*18 + f"Generated: {datetime.now().strftime('%Y-%m-%d %H:%M')}" + " "*27 + "│")
    lines.append("├" + "─"*78 + "┤")
    lines.append("│ {:24} │ {:10} │ {:10} │ {:10} │ {:10} │ {:5} │".format(
        "Model", "Accuracy", "Complete", "Tool Use", "Speed", "Runs"
    ))
    lines.append("├" + "─"*78 + "┤")
    
    # Sort by overall score
    sorted_models = sorted(
        merged["models"].items(),
        key=lambda x: x[1]["summary"].get("overall_score", 0),
        reverse=True
    )
    
    for model, data in sorted_models:
        summary = data["summary"]
        model_name = model[:24] if len(model) <= 24 else model[:21] + "..."
        
        lines.append("│ {:24} │ {:8.1f}/10 │ {:8.1f}/10 │ {:8.1f}/10 │ {:7.1f}s │ {:5} │".format(
            model_name,
            summary.get("avg_accuracy", 0),
            summary.get("avg_completeness", 0),
            summary.get("avg_tool_usage", 0),
            summary.get("avg_response_time", 0),
            summary.get("run_count", 0)
        ))
    
    lines.append("├" + "─"*78 + "┤")
    
    # Add recommendation
    if sorted_models:
        best_model = sorted_models[0][0]
        best_score = sorted_models[0][1]["summary"].get("overall_score", 0)
        lines.append("│ {:76} │".format(f"Recommended: {best_model} (score: {best_score:.1f}/10)"))
    
    lines.append("└" + "─"*78 + "┘")
    
    return "\n".join(lines)


def format_markdown(merged: dict) -> str:
    """Format results as Markdown."""
    lines = []
    
    lines.append("# YAZE AI Model Comparison Report")
    lines.append("")
    lines.append(f"Generated: {datetime.now().strftime('%Y-%m-%d %H:%M')}")
    lines.append("")
    lines.append("## Summary")
    lines.append("")
    lines.append("| Model | Accuracy | Completeness | Tool Use | Speed | Overall | Runs |")
    lines.append("|-------|----------|--------------|----------|-------|---------|------|")
    
    sorted_models = sorted(
        merged["models"].items(),
        key=lambda x: x[1]["summary"].get("overall_score", 0),
        reverse=True
    )
    
    for model, data in sorted_models:
        summary = data["summary"]
        lines.append("| {} | {:.1f}/10 | {:.1f}/10 | {:.1f}/10 | {:.1f}s | **{:.1f}/10** | {} |".format(
            model,
            summary.get("avg_accuracy", 0),
            summary.get("avg_completeness", 0),
            summary.get("avg_tool_usage", 0),
            summary.get("avg_response_time", 0),
            summary.get("overall_score", 0),
            summary.get("run_count", 0)
        ))
    
    lines.append("")
    
    # Recommendation section
    if sorted_models:
        best = sorted_models[0]
        lines.append("## Recommendation")
        lines.append("")
        lines.append(f"**Best Model:** `{best[0]}`")
        lines.append("")
        lines.append("### Strengths")
        lines.append("")
        
        summary = best[1]["summary"]
        if summary.get("avg_accuracy", 0) >= 8:
            lines.append("- ✅ High accuracy in responses")
        if summary.get("avg_tool_usage", 0) >= 8:
            lines.append("- ✅ Effective tool usage")
        if summary.get("avg_response_time", 0) <= 3:
            lines.append("- ✅ Fast response times")
        if summary.get("avg_completeness", 0) >= 8:
            lines.append("- ✅ Complete and detailed responses")
        
        lines.append("")
        lines.append("### Considerations")
        lines.append("")
        
        if summary.get("avg_accuracy", 0) < 7:
            lines.append("- ⚠️ Accuracy could be improved")
        if summary.get("avg_tool_usage", 0) < 7:
            lines.append("- ⚠️ Tool usage needs improvement")
        if summary.get("avg_response_time", 0) > 5:
            lines.append("- ⚠️ Response times are slow")
    
    # Source files section
    lines.append("")
    lines.append("## Sources")
    lines.append("")
    for source in merged.get("sources", []):
        lines.append(f"- `{source}`")
    
    return "\n".join(lines)


def format_json(merged: dict) -> str:
    """Format results as JSON."""
    # Remove internal fields
    output = {k: v for k, v in merged.items() if not k.startswith('_')}
    return json.dumps(output, indent=2)


def get_best_model(merged: dict) -> str:
    """Get the name of the best performing model."""
    sorted_models = sorted(
        merged["models"].items(),
        key=lambda x: x[1]["summary"].get("overall_score", 0),
        reverse=True
    )
    
    if sorted_models:
        return sorted_models[0][0]
    return "unknown"


def analyze_task_performance(results: list[dict]) -> dict:
    """Analyze performance broken down by task category."""
    task_performance = {}
    
    for result in results:
        for model, model_data in result.get('models', {}).items():
            for task in model_data.get('tasks', []):
                category = task.get('category', 'unknown')
                task_id = task.get('task_id', 'unknown')
                
                key = f"{category}/{task_id}"
                if key not in task_performance:
                    task_performance[key] = {
                        "category": category,
                        "task_id": task_id,
                        "task_name": task.get('task_name', 'Unknown'),
                        "models": {}
                    }
                
                if model not in task_performance[key]["models"]:
                    task_performance[key]["models"][model] = {
                        "scores": [],
                        "times": []
                    }
                
                task_performance[key]["models"][model]["scores"].append(
                    task.get('accuracy_score', 0) * 0.5 + 
                    task.get('completeness_score', 0) * 0.3 +
                    task.get('tool_usage_score', 0) * 0.2
                )
                task_performance[key]["models"][model]["times"].append(
                    task.get('response_time', 0)
                )
    
    # Calculate averages
    for task_key, task_data in task_performance.items():
        for model, model_scores in task_data["models"].items():
            scores = model_scores["scores"]
            times = model_scores["times"]
            model_scores["avg_score"] = sum(scores) / len(scores) if scores else 0
            model_scores["avg_time"] = sum(times) / len(times) if times else 0
    
    return task_performance


def format_task_analysis(task_performance: dict) -> str:
    """Format task-level analysis."""
    lines = []
    lines.append("\n## Task-Level Performance\n")
    
    # Group by category
    by_category = {}
    for key, data in task_performance.items():
        cat = data["category"]
        if cat not in by_category:
            by_category[cat] = []
        by_category[cat].append(data)
    
    for category, tasks in sorted(by_category.items()):
        lines.append(f"### {category.replace('_', ' ').title()}\n")
        lines.append("| Task | Best Model | Score | Time |")
        lines.append("|------|------------|-------|------|")
        
        for task in tasks:
            # Find best model for this task
            best_model = None
            best_score = 0
            for model, scores in task["models"].items():
                if scores["avg_score"] > best_score:
                    best_score = scores["avg_score"]
                    best_model = model
            
            if best_model:
                best_time = task["models"][best_model]["avg_time"]
                lines.append("| {} | {} | {:.1f}/10 | {:.1f}s |".format(
                    task["task_name"],
                    best_model,
                    best_score,
                    best_time
                ))
        
        lines.append("")
    
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(
        description="Generate comparison reports from AI evaluation results"
    )
    parser.add_argument(
        "files",
        nargs="+",
        help="Evaluation result JSON files to compare"
    )
    parser.add_argument(
        "--format", "-f",
        choices=["table", "markdown", "json"],
        default="table",
        help="Output format (default: table)"
    )
    parser.add_argument(
        "--output", "-o",
        help="Output file (default: stdout)"
    )
    parser.add_argument(
        "--best",
        action="store_true",
        help="Only output the best model name (for scripting)"
    )
    parser.add_argument(
        "--task-analysis",
        action="store_true",
        help="Include task-level performance analysis"
    )
    
    args = parser.parse_args()
    
    # Load and merge results
    results = load_results(args.files)
    if not results:
        print("No valid result files found", file=sys.stderr)
        sys.exit(1)
    
    merged = merge_results(results)
    
    # Handle --best flag
    if args.best:
        print(get_best_model(merged))
        sys.exit(0)
    
    # Format output
    if args.format == "table":
        output = format_table(merged)
    elif args.format == "markdown":
        output = format_markdown(merged)
        if args.task_analysis:
            task_perf = analyze_task_performance(results)
            output += format_task_analysis(task_perf)
    else:
        output = format_json(merged)
    
    # Write output
    if args.output:
        with open(args.output, 'w') as f:
            f.write(output)
        print(f"Report written to: {args.output}")
    else:
        print(output)


if __name__ == "__main__":
    main()

