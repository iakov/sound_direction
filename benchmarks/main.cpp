#include <iostream>
#include <fstream>
#include <array>

#include "trikSound/types.h"
#include "trikSound/angleDetector.h"
#include "trikSound/stereoVadFilter.h"
#include "trikSound/digitalAudioFilter.h"

#include "benchmarks.h"

using namespace std;
using namespace trikSound;


int main()
{
    ofstream fout;
    fout.open("benchmarks.out");
    ostream& out = fout;

    const int sampleRate = 44100;
    const int micrDist = 7.6;
    const int historyDepth = 20;
    const double threshold = 0.0001;

    DigitalAudioFilterPtr filter    = make_shared<DigitalAudioFilter<Iter>>();
    AngleDetectorPtr detector       = make_shared<AngleDetector<Iter>>(sampleRate, micrDist, historyDepth);
    VadFilterPtr vad                = make_shared<StereoVadFilter<Iter>>(threshold);

    BenchmarkWorker worker;
    array<int, 4> wSizes = {256, 512, 1024, 2048};

    out << "Benchmarks: iteration count = " << BenchmarkWorker::ITERATION_COUNT << endl << endl;

    for (int i = 0; i < wSizes.size(); ++i) {

        out << "Benchmarks: window size = " << wSizes[i] << endl << endl;

        worker.setWindowSize(wSizes[i]);
        benchmark_result res;

        cout << "Angle detector benchmark" << endl;

        res = worker.benchmarkAngleDetector(detector);
        out << "Angle detector benchmark " << endl;
        out << "total ms = " << res.first << endl;
        out << "average ms = " << (double) res.first / BenchmarkWorker::ITERATION_COUNT << endl;
        out << "standard deviation = " << res.second << endl;
        out << endl;

        cout << "Vad benchmark" << endl;

        res = worker.benchmarkVadFilter(vad);
        out << "Vad benchmark " << endl;
        out << "total ms = " << res.first << endl;
        out << "average ms = " << (double) res.first / BenchmarkWorker::ITERATION_COUNT << endl;
        out << "standard deviation = " << res.second << endl;
        out << endl;

        cout << "Filter benchmark " << endl;

        res = worker.benchmarkDigitalFilter(filter);
        out << "Filter benchmark " << endl;
        out << "total ms = " << res.first << endl;
        out << "average ms = " << (double) res.first / BenchmarkWorker::ITERATION_COUNT << endl;
        out << "standard deviation = " << res.second << endl;
        out << endl;
    }

    return 0;
}

