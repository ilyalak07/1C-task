// Карта читается из stdin до строки END, дальше по stdin/stdout идёт диалог

#include <algorithm>
#include <functional>
#include <iostream>
#include <limits>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{

    // карта

    struct Tunnel
    {
        std::string id;
        int a = 0;
        int b = 0;
        double p = 0.0;

        int other(int hall) const { return hall == a ? b : a; }
    };

    struct Map
    {
        double dayCost = 0.0;
        int entrance = -1;
        std::vector<std::string> name;
        std::vector<double> profit;
        std::vector<Tunnel> tunnels;
        std::vector<std::vector<int>> incident; // ходы каждого зала
        std::unordered_map<std::string, int> hallId;
        std::unordered_map<std::string, int> tunnelId;

        int halls() const { return static_cast<int>(name.size()); }
    };

    double toNumber(const std::string &text)
    {
        std::size_t used = 0;
        const double value = std::stod(text, &used);
        if (used != text.size())
            throw std::runtime_error("bad number '" + text + "'");
        return value;
    }

    // формат как в услоии
    Map readMap(std::istream &in)
    {
        Map map;
        std::string entranceName;
        std::vector<std::vector<std::string>> edges;
        std::string line;
        while (std::getline(in, line))
        {
            std::istringstream words(line);
            std::vector<std::string> w;
            for (std::string token; words >> token;)
                w.push_back(token);
            if (w.empty() || w[0] == "MAP")
                continue;
            if (w[0] == "END")
                break;
            if (w[0] == "day_cost" && w.size() == 2)
            {
                map.dayCost = toNumber(w[1]);
            }
            else if (w[0] == "entrance" && w.size() == 2)
            {
                entranceName = w[1];
            }
            else if (w[0] == "vertex" && w.size() == 3)
            {
                map.hallId[w[1]] = map.halls();
                map.name.push_back(w[1]);
                map.profit.push_back(toNumber(w[2]));
                map.incident.emplace_back();
            }
            else if (w[0] == "edge" && w.size() == 5)
            {
                edges.push_back(w);
            }
            else
            {
                throw std::runtime_error("bad map line: " + line);
            }
        }
        const auto hall = [&](const std::string &n)
        {
            const auto it = map.hallId.find(n);
            if (it == map.hallId.end())
                throw std::runtime_error("unknown hall '" + n + "'");
            return it->second;
        };
        map.entrance = hall(entranceName);
        for (const auto &e : edges)
        {
            const Tunnel t{e[1], hall(e[2]), hall(e[3]), toNumber(e[4])};
            const int id = static_cast<int>(map.tunnels.size());
            map.tunnelId[t.id] = id;
            map.incident[t.a].push_back(id);
            if (t.b != t.a)
                map.incident[t.b].push_back(id);
            map.tunnels.push_back(t);
        }
        return map;
    }

    class Planner
    {
    public:
        explicit Planner(const Map &map)
            : map_(map), dist_(map.halls()), subtree_(map.halls()), parent_(map.halls()), via_(map.halls(), -1) {}

        int decide(const std::vector<char> &reached)
        {
            int unreached = 0;
            for (const char r : reached)
                unreached += r ? 0 : 1;
            if (unreached > kBigGraph)
                return decideFast(reached);

            const double cost = map_.dayCost;
            const double treeValue = evaluate(reached, -1);

            std::vector<std::pair<double, int>> candidates;
            for (int x = 0; x < map_.halls(); ++x)
            {
                if (!reached[x])
                    continue;
                for (const int t : map_.incident[x])
                {
                    const int y = map_.tunnels[t].other(x);
                    if (!reached[y] && via_[y] < 0)
                    {
                        via_[y] = t;
                        candidates.emplace_back(subtree_[y], y);
                    }
                }
            }
            if (candidates.empty())
                return -1;
            std::sort(candidates.begin(), candidates.end(), std::greater<>());

            double best = kEps * (1.0 + cost);
            int bestHall = -1;
            work_ = reached;
            for (int i = 0; i < std::min<int>(kCandidates, static_cast<int>(candidates.size())); ++i)
            {
                const int v = candidates[i].second;
                work_[v] = 1;
                const double q = map_.profit[v] - cost + evaluate(work_, v);
                work_[v] = 0;
                if (q > best)
                {
                    best = q;
                    bestHall = v;
                }
            }
            if (bestHall < 0 && treeValue > kEps * (1.0 + cost))
                bestHall = candidates.front().second;

            const int tunnel = bestHall < 0 ? -1 : via_[bestHall];
            for (const auto &c : candidates)
                via_[c.second] = -1;
            return tunnel;
        }

    private:
        static constexpr int kCandidates = 8;
        static constexpr int kBigGraph = 3000;
        static constexpr double kEps = 1e-9;

        double evaluate(const std::vector<char> &reached, int fresh)
        {
            using Item = std::pair<double, int>;
            std::fill(dist_.begin(), dist_.end(), std::numeric_limits<double>::infinity());
            std::priority_queue<Item, std::vector<Item>, std::greater<>> heap;
            order_.clear();
            const auto relax = [&](int v, double d, int tunnel)
            {
                if (d < dist_[v])
                {
                    dist_[v] = d;
                    parent_[v] = tunnel;
                    heap.emplace(d, v);
                }
            };
            for (int x = 0; x < map_.halls(); ++x)
            {
                if (!reached[x])
                    continue;
                for (const int t : map_.incident[x])
                {
                    const int y = map_.tunnels[t].other(x);
                    if (!reached[y])
                        relax(y, x == fresh ? 1.0 - map_.tunnels[t].p : 1.0, t);
                }
            }
            while (!heap.empty())
            {
                const auto [d, y] = heap.top();
                heap.pop();
                if (d > dist_[y])
                    continue;
                order_.push_back(y);
                for (const int t : map_.incident[y])
                {
                    const int z = map_.tunnels[t].other(y);
                    if (!reached[z])
                        relax(z, d + 1.0 - map_.tunnels[t].p, t);
                }
            }

            double total = 0.0;
            for (const int y : order_)
                subtree_[y] = map_.profit[y];
            for (auto it = order_.rbegin(); it != order_.rend(); ++it)
            {
                const Tunnel &t = map_.tunnels[parent_[*it]];
                const int up = t.other(*it);
                const double p = (reached[up] && up != fresh) ? 0.0 : t.p;
                const double s = subtree_[*it];
                const double gain = p * s + (1.0 - p) * std::max(0.0, s - map_.dayCost);
                (reached[up] ? total : subtree_[up]) += gain;
            }
            return total;
        }

        int decideFast(const std::vector<char> &reached)
        {
            for (int attempt = 0; attempt < 2; ++attempt)
            {
                for (int v = 0; v < map_.halls(); ++v)
                {
                    if (!reached[v] && fixedParent_.size() == reached.size() && fixedParent_[v] >= 0 &&
                        reached[map_.tunnels[fixedParent_[v]].other(v)] && fixedGain_[v] > kEps * (1.0 + map_.dayCost))
                    {
                        return fixedParent_[v];
                    }
                }
                if (attempt == 0)
                {
                    evaluate(reached, -1);
                    fixedParent_.assign(map_.halls(), -1);
                    fixedGain_.assign(map_.halls(), 0.0);
                    for (const int y : order_)
                    {
                        fixedParent_[y] = parent_[y];
                        fixedGain_[y] = subtree_[y] - map_.dayCost;
                    }
                }
            }
            return -1;
        }

        const Map &map_;
        std::vector<double> dist_, subtree_;
        std::vector<int> parent_, order_, via_;
        std::vector<char> work_;
        std::vector<int> fixedParent_;
        std::vector<double> fixedGain_;
    };

    class Expedition
    {
    public:
        Expedition(const Map &map, std::istream &in, std::ostream &out)
            : map_(map), in_(in), out_(out), planner_(map), reached_(map.halls(), 0), looked_(map.halls(), 0),
              intact_(map.tunnels.size(), 0), parent_(map.halls(), -1), depth_(map.halls(), 0), position_(map.entrance) {}

        void run()
        {
            reached_[position_] = 1;
            explore();
            std::vector<char> cleared(map_.tunnels.size(), 0);
            for (int t; (t = planner_.decide(reached_)) >= 0;)
            {
                int from = map_.tunnels[t].a;
                int to = map_.tunnels[t].b;
                if (!reached_[from])
                    std::swap(from, to);
                if (cleared[t] || !reached_[from] || reached_[to])
                    break;
                cleared[t] = 1;
                send("CLEAR " + map_.tunnels[t].id);
                readLine();
                intact_[t] = 1;
                position_ = map_.entrance; // смена кончается на поверхности
                walkTo(from);
                if (move(t))
                {
                    enter(to, t);
                    explore();
                }
            }
            send("END");
            std::string result;
            if (std::getline(in_, result))
                std::cerr << result << '\n';
        }

    private:
        void send(const std::string &command) { out_ << command << std::endl; }

        std::string readLine()
        {
            std::string line;
            while (std::getline(in_, line))
            {
                if (!line.empty() && line.back() == '\r')
                    line.pop_back();
                if (line.find_first_not_of(" \t") != std::string::npos)
                    return line;
            }
            throw std::runtime_error("judge closed the stream");
        }

        void look()
        {
            send("LOOK");
            for (std::size_t i = 0; i < map_.incident[position_].size(); ++i)
            {
                std::istringstream answer(readLine());
                std::string word, id, hall, state;
                answer >> word >> id >> hall >> state;
                const auto it = map_.tunnelId.find(id);
                if (word != "PATH" || it == map_.tunnelId.end())
                    throw std::runtime_error("bad LOOK answer");
                intact_[it->second] = state == "OK";
            }
            looked_[position_] = 1;
        }

        bool move(int t)
        {
            send("MOVE " + map_.tunnels[t].id);
            if (readLine().rfind("OK", 0) != 0)
            {
                intact_[t] = 0;
                return false;
            }
            position_ = map_.tunnels[t].other(position_);
            return true;
        }

        void enter(int hall, int viaTunnel)
        {
            reached_[hall] = 1;
            parent_[hall] = viaTunnel;
            depth_[hall] = depth_[map_.tunnels[viaTunnel].other(hall)] + 1;
        }

        void explore()
        {
            std::vector<std::pair<int, std::size_t>> stack{{position_, 0}};
            while (!stack.empty())
            {
                const int hall = stack.back().first;
                if (!looked_[hall])
                    look();
                bool descended = false;
                for (auto &cursor = stack.back().second; cursor < map_.incident[hall].size();)
                {
                    const int t = map_.incident[hall][cursor++];
                    const int next = map_.tunnels[t].other(hall);
                    if (intact_[t] && !reached_[next] && move(t))
                    {
                        enter(next, t);
                        stack.emplace_back(next, 0);
                        descended = true;
                        break;
                    }
                }
                if (descended)
                    continue;
                stack.pop_back();
                if (!stack.empty())
                    move(parent_[hall]); // назад по своим следам
            }
        }

        // Путь по дереву первых посещений: вверх до общего предка, потом вниз
        void walkTo(int target)
        {
            std::vector<int> up, down;
            for (int a = position_, b = target; a != b;)
            {
                if (depth_[a] >= depth_[b])
                {
                    up.push_back(parent_[a]);
                    a = map_.tunnels[parent_[a]].other(a);
                }
                else
                {
                    down.push_back(parent_[b]);
                    b = map_.tunnels[parent_[b]].other(b);
                }
            }
            for (const int t : up)
                move(t);
            for (auto it = down.rbegin(); it != down.rend(); ++it)
                move(*it);
        }

        const Map &map_;
        std::istream &in_;
        std::ostream &out_;
        Planner planner_;
        std::vector<char> reached_, looked_, intact_;
        std::vector<int> parent_, depth_; // дерево первых посещений
        int position_;
    };

}

int main()
{
    std::ios::sync_with_stdio(false);
    try
    {
        const Map map = readMap(std::cin);
        Expedition(map, std::cin, std::cout).run();
    }
    catch (const std::exception &error)
    {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
