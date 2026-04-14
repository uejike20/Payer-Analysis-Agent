FROM gcc:14 AS build

WORKDIR /src

COPY AnalysisPipeline ./AnalysisPipeline

RUN mkdir -p /out && g++ -std=c++17 -O2 \
    AnalysisPipeline/main.cpp \
    AnalysisPipeline/Payers.cpp \
    AnalysisPipeline/ProviderManual.cpp \
    AnalysisPipeline/PhoneTranscript.cpp \
    AnalysisPipeline/WebPage.cpp \
    AnalysisPipeline/DenialLetter.cpp \
    -o /out/authpipeline

FROM gcc:14 AS runtime

WORKDIR /app/AnalysisPipeline

RUN useradd -m -u 1000 appuser

COPY --from=build /out/authpipeline /usr/local/bin/authpipeline
COPY --from=build /src/AnalysisPipeline/payer_sources ./payer_sources
COPY --from=build /src/AnalysisPipeline/reconcile_latest.py ./reconcile_latest.py
COPY --from=build /src/AnalysisPipeline/format_best_answer.py ./format_best_answer.py
COPY --from=build /src/AnalysisPipeline/run_pipeline.sh /usr/local/bin/run_pipeline.sh

RUN chmod +x /usr/local/bin/run_pipeline.sh && chown -R appuser:appuser /app/AnalysisPipeline

USER 1000:1000

ENTRYPOINT ["/usr/local/bin/run_pipeline.sh"]
