//MPI phonebook search code, first-letter anchored fuzzy matching

#include <bits/stdc++.h>
#include <mpi.h>

using namespace std;

/* -------- MPI STRING SEND / RECEIVE -------- */

void send_string(const string &text, int receiver) {
    int len = text.size() + 1;
    MPI_Send(&len, 1, MPI_INT, receiver, 10, MPI_COMM_WORLD);
    MPI_Send(text.c_str(), len, MPI_CHAR, receiver, 11, MPI_COMM_WORLD);
}

string receive_string(int sender) {
    int len;
    MPI_Status status;

    MPI_Recv(&len, 1, MPI_INT, sender, 10, MPI_COMM_WORLD, &status);
    char *buf = new char[len];
    MPI_Recv(buf, len, MPI_CHAR, sender, 11, MPI_COMM_WORLD, &status);

    string res(buf);
    delete[] buf;
    return res;
}

/* -------- STRING HELPERS -------- */

string vector_to_string(const vector<string> &lines, int start, int end) {
    string result;
    for (int i = start; i < min((int)lines.size(), end); i++) {
        result += lines[i] + "\n";
    }
    return result;
}

vector<string> string_to_vector(const string &text) {
    vector<string> lines;
    istringstream iss(text);
    string line;
    while (getline(iss, line)) {
        if (!line.empty())
            lines.push_back(line);
    }
    return lines;
}

/* -------- FUZZY / APPROXIMATE MATCH (FIRST LETTER ANCHORED) -------- */

string check(const string &line, const string &search) {
    const int MIN_MATCH = 4;

    int n = line.size();
    int m = search.size();

    for (int start = 0; start <= n - MIN_MATCH; start++) {

        // first letter anchor
        if (tolower(line[start]) != tolower(search[0]))
            continue;

        int k = 0;
        // strict forward matching
        while (start + k < n && k < m &&
               tolower(line[start + k]) == tolower(search[k])) {
            k++;
        }

        if (k >= MIN_MATCH) {
            return line + "\n";
        }
    }

    return "";
}


/* -------- FILE READING -------- */

void read_phonebook(const vector<string> &files, vector<string> &lines) {
    for (const string &file : files) {
        ifstream f(file);
        if (!f.is_open()) {
            cerr << "Could not open file: " << file << endl;
            continue;
        }
        string line;
        while (getline(f, line)) {
            if (!line.empty())
                lines.push_back(line);
        }
    }
}

/* -------- MAIN -------- */

int main(int argc, char **argv) {

    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc < 3) {
        if (rank == 0) {
            cerr << "Usage: mpirun -np <procs> "
                 << argv[0]
                 << " <file1> <file2> ... <search_term>\n";
        }
        MPI_Finalize();
        return 1;
    }

    string search_term = argv[argc - 1];
    double start_time, end_time;

    /* -------- MASTER -------- */
    if (rank == 0) {

        vector<string> files;
        for (int i = 1; i < argc - 1; i++)
            files.push_back(argv[i]);

        vector<string> all_lines;
        read_phonebook(files, all_lines);

        int total = all_lines.size();
        int chunk = (total + size - 1) / size;

        // send chunks to workers
        for (int i = 1; i < size; i++) {
            string text_chunk =
                vector_to_string(all_lines, i * chunk, (i + 1) * chunk);
            send_string(text_chunk, i);
        }

        // master search
        start_time = MPI_Wtime();
        string final_result;

        for (int i = 0; i < min(chunk, total); i++) {
            string match = check(all_lines[i], search_term);
            if (!match.empty())
                final_result += match;
        }

        // receive worker results
        for (int i = 1; i < size; i++) {
            string worker_result = receive_string(i);
            final_result += worker_result;
        }

        end_time = MPI_Wtime();

        ofstream out("output.txt");
        out << final_result;
        out.close();

        cout << "Search complete. Results saved to output.txt\n";
        printf("Total execution time: %f seconds\n",
               end_time - start_time);
    }

    /* -------- WORKER -------- */
    else {
        string recv_text = receive_string(0);
        vector<string> local_lines = string_to_vector(recv_text);

        start_time = MPI_Wtime();
        string local_matches;

        for (const string &line : local_lines) {
            string match = check(line, search_term);
            if (!match.empty())
                local_matches += match;
        }

        end_time = MPI_Wtime();

        send_string(local_matches, 0);

        printf("Process %d finished in %f seconds\n",
               rank, end_time - start_time);
    }

    MPI_Finalize();
    return 0;
}
//mpic++ phone4.cpp -o p
//mpirun -np 4 ./p phonebook.txt TAMAk
