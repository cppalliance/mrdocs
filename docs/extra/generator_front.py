import matplotlib.pyplot as plt
import os

# Data points for formats (Redundancy, Convenience)
formats = {
    "Clang AST": (0.9, 0.0),  # High redundancy, low convenience
    "XML": (0, 0.1),  # Slightly more redundancy and convenience
    "Asciidoc": (0.8, 0.8),  # Higher convenience, higher redundancy
    "HTML": (0.9, 1.0),  # Most convenient, slightly more redundant
    "Custom Templates": (0.5, 0.6),  # Most convenient, slightly more redundant
}

# Hypothetical solutions behind the Pareto front
hypotheticals = [(0.4, 0.05), (0.45, 0.05), (0.7, 0.2), (0.9, 0.45), (1, 0.35)]

# Plotting
plt.figure(figsize=(8, 6))


def calculate_pareto_front(data_points):
    # Sort data points by the x-coordinate
    data_points.sort()

    # Calculate the Pareto front
    pareto_front = []
    max_y = float('-inf')
    for x, y in data_points:
        if y > max_y:
            pareto_front.append((x, y))
            max_y = y

    # Create stairs for the Pareto front
    min_x = min(data_points, key=lambda x: x[0])[0]
    max_x = max(data_points, key=lambda x: x[0])[0]
    min_y = min(data_points, key=lambda x: x[1])[1]
    max_y = max(data_points, key=lambda x: x[1])[1]
    stairs_x = [min_x]
    stairs_y = [min_y]
    for i in range(len(pareto_front) - 1):
        stairs_x.extend([pareto_front[i][0], pareto_front[i + 1][0]])
        stairs_y.extend([pareto_front[i][1], pareto_front[i][1]])
    stairs_x.append(pareto_front[-1][0])
    stairs_y.append(pareto_front[-1][1])
    stairs_x.append(max_x)
    stairs_y.append(max_y)

    return stairs_x, stairs_y


data_points = list(formats.values()) + hypotheticals
pareto_x, pareto_y = calculate_pareto_front(data_points)
plt.plot(pareto_x, pareto_y, linestyle="--", color="gray", label="Best Trade-offs")

# Hypothetical solutions
x_hyp, y_hyp = zip(*hypotheticals)
plt.scatter(x_hyp, y_hyp, color="gray", alpha=0.5, label="Hypothetical")

for label, (x, y) in formats.items():
    plt.scatter(x, y, s=100)
    plt.annotate(label, (x, y), textcoords="offset points", xytext=(5, 5), ha='center')

# The ideal solution
plt.scatter(0, 1, color="gray", alpha=0.5)
plt.annotate("Ideal", (0, 1), textcoords="offset points", xytext=(5, 5), ha='center')

# Labels and Legend
plt.xlabel("Redundancy")
plt.ylabel("Convenience")
plt.title("Output Formats")
plt.legend()
plt.grid(True)
plt.xticks([])
plt.yticks([])

# Get the directory of the script
script_dir = os.path.dirname(__file__)

# Construct the target path
target_path = os.path.join(script_dir, '../modules/ROOT/images/generator_front.svg')

# Save the plot to the target path
plt.savefig(target_path)