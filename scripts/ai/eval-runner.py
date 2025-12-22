#!/usr/bin/env python3
"""
YAZE AI Model Evaluation Runner

Runs evaluation tasks against multiple AI models and produces scored results.

Usage:
    python eval-runner.py --models llama3,qwen2.5-coder --tasks rom_inspection
    python eval-runner.py --all-models --tasks all --output results/eval-$(date +%Y%m%d).json

Requirements:
    pip install requests pyyaml
"""

import argparse
import json
import os
import re
import subprocess
import sys
import time
from dataclasses import asdict, dataclass, field
from datetime import datetime
from pathlib import Path
from typing import Any, Optional

import requests
import yaml


@dataclass
class TaskResult:
    """Result of a single task evaluation."""
    task_id: str
    task_name: str
    category: str
    model: str
    prompt: str
    response: str
    response_time: float
    accuracy_score: float = 0.0
    completeness_score: float = 0.0
    tool_usage_score: float = 0.0
    pattern_matches: list = field(default_factory=list)
    tools_used: list = field(default_factory=list)
    error: Optional[str] = None
    
    @property
    def overall_score(self) -> float:
        """Calculate weighted overall score."""
        # Default weights from eval-tasks.yaml
        weights = {
            'accuracy': 0.4,
            'completeness': 0.3,
            'tool_usage': 0.2,
            'response_time': 0.1
        }
        
        # Normalize response time to 0-10 scale (lower is better)
        # 0s = 10, 60s+ = 0
        time_score = max(0, 10 - (self.response_time / 6))
        
        return (
            weights['accuracy'] * self.accuracy_score +
            weights['completeness'] * self.completeness_score +
            weights['tool_usage'] * self.tool_usage_score +
            weights['response_time'] * time_score
        )


@dataclass
class ModelResults:
    """Aggregated results for a single model."""
    model: str
    tasks: list[TaskResult] = field(default_factory=list)
    
    @property
    def avg_accuracy(self) -> float:
        if not self.tasks:
            return 0.0
        return sum(t.accuracy_score for t in self.tasks) / len(self.tasks)
    
    @property
    def avg_completeness(self) -> float:
        if not self.tasks:
            return 0.0
        return sum(t.completeness_score for t in self.tasks) / len(self.tasks)
    
    @property
    def avg_tool_usage(self) -> float:
        if not self.tasks:
            return 0.0
        return sum(t.tool_usage_score for t in self.tasks) / len(self.tasks)
    
    @property
    def avg_response_time(self) -> float:
        if not self.tasks:
            return 0.0
        return sum(t.response_time for t in self.tasks) / len(self.tasks)
    
    @property
    def overall_score(self) -> float:
        if not self.tasks:
            return 0.0
        return sum(t.overall_score for t in self.tasks) / len(self.tasks)


class OllamaClient:
    """Client for Ollama API."""
    
    def __init__(self, base_url: str = "http://localhost:11434"):
        self.base_url = base_url
        
    def is_available(self) -> bool:
        """Check if Ollama is running."""
        try:
            resp = requests.get(f"{self.base_url}/api/tags", timeout=5)
            return resp.status_code == 200
        except requests.exceptions.RequestException:
            return False
    
    def list_models(self) -> list[str]:
        """List available models."""
        try:
            resp = requests.get(f"{self.base_url}/api/tags", timeout=10)
            if resp.status_code == 200:
                data = resp.json()
                return [m['name'] for m in data.get('models', [])]
        except requests.exceptions.RequestException:
            pass
        return []
    
    def pull_model(self, model: str) -> bool:
        """Pull a model if not available."""
        print(f"  Pulling model {model}...", end=" ", flush=True)
        try:
            resp = requests.post(
                f"{self.base_url}/api/pull",
                json={"name": model},
                timeout=600  # 10 minutes for large models
            )
            if resp.status_code == 200:
                print("Done")
                return True
        except requests.exceptions.RequestException as e:
            print(f"Failed: {e}")
        return False
    
    def chat(self, model: str, prompt: str, timeout: int = 120) -> tuple[str, float]:
        """
        Send a chat message and return response + response time.
        
        Returns:
            Tuple of (response_text, response_time_seconds)
        """
        start_time = time.time()
        
        try:
            resp = requests.post(
                f"{self.base_url}/api/chat",
                json={
                    "model": model,
                    "messages": [{"role": "user", "content": prompt}],
                    "stream": False
                },
                timeout=timeout
            )
            
            elapsed = time.time() - start_time
            
            if resp.status_code == 200:
                data = resp.json()
                content = data.get("message", {}).get("content", "")
                return content, elapsed
            else:
                return f"Error: HTTP {resp.status_code}", elapsed
                
        except requests.exceptions.Timeout:
            return "Error: Request timed out", timeout
        except requests.exceptions.RequestException as e:
            return f"Error: {str(e)}", time.time() - start_time


class TaskEvaluator:
    """Evaluates task responses and assigns scores."""
    
    def __init__(self, config: dict):
        self.config = config
        
    def evaluate(self, task: dict, response: str, response_time: float) -> TaskResult:
        """Evaluate a response for a task."""
        result = TaskResult(
            task_id=task['id'],
            task_name=task['name'],
            category=task.get('category', 'unknown'),
            model=task.get('model', 'unknown'),
            prompt=task.get('prompt', ''),
            response=response,
            response_time=response_time
        )
        
        if response.startswith("Error:"):
            result.error = response
            return result
        
        # Check pattern matches
        expected_patterns = task.get('expected_patterns', [])
        for pattern in expected_patterns:
            if re.search(pattern, response, re.IGNORECASE):
                result.pattern_matches.append(pattern)
        
        # Score accuracy based on pattern matches
        if expected_patterns:
            match_ratio = len(result.pattern_matches) / len(expected_patterns)
            result.accuracy_score = match_ratio * 10
        else:
            # No patterns defined, give neutral score
            result.accuracy_score = 5.0
        
        # Score completeness based on response length and structure
        result.completeness_score = self._score_completeness(response, task)
        
        # Score tool usage
        result.tool_usage_score = self._score_tool_usage(response, task)
        
        return result
    
    def _score_completeness(self, response: str, task: dict) -> float:
        """Score completeness based on response characteristics."""
        score = 0.0
        
        # Base score for having a response
        if len(response.strip()) > 0:
            score += 2.0
        
        # Length bonus (up to 4 points)
        word_count = len(response.split())
        if word_count >= 20:
            score += min(4.0, word_count / 50)
        
        # Structure bonus (up to 2 points)
        if '\n' in response:
            score += 1.0  # Multi-line response
        if '- ' in response or '* ' in response:
            score += 0.5  # List items
        if any(c.isdigit() for c in response):
            score += 0.5  # Contains numbers/data
        
        # Code block bonus
        if '```' in response or '    ' in response:
            score += 1.0
        
        return min(10.0, score)
    
    def _score_tool_usage(self, response: str, task: dict) -> float:
        """Score tool usage based on task requirements."""
        required_tool = task.get('required_tool')
        
        if not required_tool:
            # No tool required, check if response is sensible
            return 7.0  # Neutral-good score
        
        # Check if the response mentions using tools
        tool_patterns = [
            r'filesystem-list',
            r'filesystem-read',
            r'filesystem-exists',
            r'filesystem-info',
            r'build-configure',
            r'build-compile',
            r'build-test',
            r'memory-analyze',
            r'memory-search',
        ]
        
        tools_mentioned = []
        for pattern in tool_patterns:
            if re.search(pattern, response, re.IGNORECASE):
                tools_mentioned.append(pattern)
        
        if required_tool.lower() in ' '.join(tools_mentioned).lower():
            return 10.0  # Used the required tool
        elif tools_mentioned:
            return 6.0  # Used some tools but not the required one
        else:
            return 3.0  # Didn't use any tools when one was required


def load_config(config_path: str) -> dict:
    """Load the evaluation tasks configuration."""
    with open(config_path, 'r') as f:
        return yaml.safe_load(f)


def get_tasks_for_categories(config: dict, categories: list[str]) -> list[dict]:
    """Get all tasks for specified categories."""
    tasks = []
    
    for cat_name, cat_data in config.get('categories', {}).items():
        if 'all' in categories or cat_name in categories:
            for task in cat_data.get('tasks', []):
                task['category'] = cat_name
                tasks.append(task)
    
    return tasks


def run_evaluation(
    models: list[str],
    tasks: list[dict],
    client: OllamaClient,
    evaluator: TaskEvaluator,
    timeout: int = 120
) -> dict[str, ModelResults]:
    """Run evaluation for all models and tasks."""
    results = {}
    
    total = len(models) * len(tasks)
    current = 0
    
    for model in models:
        print(f"\n{'='*60}")
        print(f"Evaluating: {model}")
        print(f"{'='*60}")
        
        model_results = ModelResults(model=model)
        
        for task in tasks:
            current += 1
            print(f"\n  [{current}/{total}] {task['id']}: {task['name']}")
            
            # Handle multi-turn tasks differently
            if task.get('multi_turn'):
                response, resp_time = run_multi_turn_task(
                    client, model, task, timeout
                )
            else:
                prompt = task.get('prompt', '')
                print(f"    Prompt: {prompt[:60]}...")
                response, resp_time = client.chat(model, prompt, timeout)
            
            print(f"    Response time: {resp_time:.2f}s")
            
            # Create a copy of task with model info
            task_with_model = {**task, 'model': model}
            
            # Evaluate the response
            result = evaluator.evaluate(task_with_model, response, resp_time)
            model_results.tasks.append(result)
            
            print(f"    Accuracy: {result.accuracy_score:.1f}/10")
            print(f"    Completeness: {result.completeness_score:.1f}/10")
            print(f"    Tool Usage: {result.tool_usage_score:.1f}/10")
            print(f"    Overall: {result.overall_score:.1f}/10")
        
        results[model] = model_results
    
    return results


def run_multi_turn_task(
    client: OllamaClient,
    model: str,
    task: dict,
    timeout: int
) -> tuple[str, float]:
    """Run a multi-turn conversation task."""
    prompts = task.get('prompts', [])
    if not prompts:
        return "Error: No prompts defined for multi-turn task", 0.0
    
    total_time = 0.0
    all_responses = []
    
    for i, prompt in enumerate(prompts):
        # For simplicity, we send each prompt independently
        # A more sophisticated version would maintain conversation context
        print(f"    Turn {i+1}: {prompt[:50]}...")
        response, resp_time = client.chat(model, prompt, timeout)
        total_time += resp_time
        all_responses.append(f"Turn {i+1}: {response}")
    
    return "\n\n".join(all_responses), total_time


def print_summary(results: dict[str, ModelResults]):
    """Print a summary table of results."""
    print("\n")
    print("┌" + "─"*70 + "┐")
    print("│" + " "*20 + "YAZE AI Model Evaluation Report" + " "*18 + "│")
    print("├" + "─"*70 + "┤")
    print("│ {:20} │ {:10} │ {:10} │ {:10} │ {:10} │".format(
        "Model", "Accuracy", "Tool Use", "Speed", "Overall"
    ))
    print("├" + "─"*70 + "┤")
    
    for model, model_results in sorted(
        results.items(),
        key=lambda x: x[1].overall_score,
        reverse=True
    ):
        # Format model name (truncate if needed)
        model_name = model[:20] if len(model) <= 20 else model[:17] + "..."
        
        print("│ {:20} │ {:8.1f}/10 │ {:8.1f}/10 │ {:7.1f}s │ {:8.1f}/10 │".format(
            model_name,
            model_results.avg_accuracy,
            model_results.avg_tool_usage,
            model_results.avg_response_time,
            model_results.overall_score
        ))
    
    print("└" + "─"*70 + "┘")


def save_results(results: dict[str, ModelResults], output_path: str):
    """Save detailed results to JSON file."""
    output_data = {
        "timestamp": datetime.now().isoformat(),
        "version": "1.0",
        "models": {}
    }
    
    for model, model_results in results.items():
        output_data["models"][model] = {
            "summary": {
                "avg_accuracy": model_results.avg_accuracy,
                "avg_completeness": model_results.avg_completeness,
                "avg_tool_usage": model_results.avg_tool_usage,
                "avg_response_time": model_results.avg_response_time,
                "overall_score": model_results.overall_score,
            },
            "tasks": [asdict(t) for t in model_results.tasks]
        }
    
    os.makedirs(os.path.dirname(output_path) or '.', exist_ok=True)
    with open(output_path, 'w') as f:
        json.dump(output_data, f, indent=2)
    
    print(f"\nResults saved to: {output_path}")


def main():
    parser = argparse.ArgumentParser(
        description="YAZE AI Model Evaluation Runner"
    )
    parser.add_argument(
        "--models", "-m",
        type=str,
        help="Comma-separated list of models to evaluate"
    )
    parser.add_argument(
        "--all-models",
        action="store_true",
        help="Evaluate all available models"
    )
    parser.add_argument(
        "--default-models",
        action="store_true",
        help="Evaluate default models from config"
    )
    parser.add_argument(
        "--tasks", "-t",
        type=str,
        default="all",
        help="Task categories to run (comma-separated, or 'all')"
    )
    parser.add_argument(
        "--config", "-c",
        type=str,
        default=os.path.join(os.path.dirname(__file__), "eval-tasks.yaml"),
        help="Path to evaluation config file"
    )
    parser.add_argument(
        "--output", "-o",
        type=str,
        help="Output file for results (default: results/eval-TIMESTAMP.json)"
    )
    parser.add_argument(
        "--timeout",
        type=int,
        default=120,
        help="Timeout in seconds for each task (default: 120)"
    )
    parser.add_argument(
        "--ollama-url",
        type=str,
        default="http://localhost:11434",
        help="Ollama API URL"
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Show what would be evaluated without running"
    )
    
    args = parser.parse_args()
    
    # Load configuration
    print("Loading configuration...")
    try:
        config = load_config(args.config)
    except Exception as e:
        print(f"Error loading config: {e}")
        sys.exit(1)
    
    # Initialize Ollama client
    client = OllamaClient(args.ollama_url)
    
    if not client.is_available():
        print("Error: Ollama is not running. Start it with 'ollama serve'")
        sys.exit(1)
    
    # Determine which models to evaluate
    available_models = client.list_models()
    print(f"Available models: {', '.join(available_models) or 'none'}")
    
    if args.all_models:
        models = available_models
    elif args.default_models:
        default_model_names = [
            m['name'] for m in config.get('default_models', [])
        ]
        models = [m for m in default_model_names if m in available_models]
        # Offer to pull missing models
        missing = [m for m in default_model_names if m not in available_models]
        if missing:
            print(f"Missing default models: {', '.join(missing)}")
            for m in missing:
                if client.pull_model(m):
                    models.append(m)
    elif args.models:
        models = [m.strip() for m in args.models.split(',')]
        # Validate models exist
        for m in models:
            if m not in available_models:
                print(f"Warning: Model '{m}' not found. Attempting to pull...")
                if not client.pull_model(m):
                    print(f"  Failed to pull {m}, skipping")
                    models.remove(m)
    else:
        # Default to first available model
        models = available_models[:1] if available_models else []
    
    if not models:
        print("No models available for evaluation")
        sys.exit(1)
    
    print(f"Models to evaluate: {', '.join(models)}")
    
    # Get tasks
    categories = [c.strip() for c in args.tasks.split(',')]
    tasks = get_tasks_for_categories(config, categories)
    
    if not tasks:
        print(f"No tasks found for categories: {args.tasks}")
        sys.exit(1)
    
    print(f"Tasks to run: {len(tasks)}")
    for task in tasks:
        print(f"  - [{task['category']}] {task['id']}: {task['name']}")
    
    if args.dry_run:
        print("\nDry run complete. Use --help for options.")
        sys.exit(0)
    
    # Run evaluation
    evaluator = TaskEvaluator(config)
    results = run_evaluation(
        models, tasks, client, evaluator, args.timeout
    )
    
    # Print summary
    print_summary(results)
    
    # Save results
    output_path = args.output or os.path.join(
        os.path.dirname(__file__),
        "results",
        f"eval-{datetime.now().strftime('%Y%m%d-%H%M%S')}.json"
    )
    save_results(results, output_path)
    
    # Return exit code based on best model score
    best_score = max(r.overall_score for r in results.values())
    if best_score >= 7.0:
        sys.exit(0)  # Good
    elif best_score >= 5.0:
        sys.exit(1)  # Okay
    else:
        sys.exit(2)  # Poor


if __name__ == "__main__":
    main()

