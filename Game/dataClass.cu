#include "dataClass.cuh"

#include <CUDA/include/cuda.h>
#include <CUDA/cmath>

#include <iostream>
#include <stdio.h>	
#include "microPhysics.cuh"

//Graph plotting
#include "core/engine.hpp"
#include <imgui/imgui.h>
#include <imgui/implot.h>

dataClass::dataClass()
{
	microPhysEnvDataResult.reset();
}

dataClass::~dataClass()
{
}



void dataClass::confirmMicroPhysCheckRegion(const int2 minCorner, const int2 maxCorner)
{
	microPhysEnvDataResult.reset();
	microPhysMinPos = minCorner;
	microPhysMaxPos = maxCorner;
	microPhysCheckActive = true;
}

void dataClass::cancelMicroPhysCheckRegion()
{
	microPhysCheckActive = false;
	microPhysMinPos = { -1,-1 };
	microPhysMaxPos = { -1,-1 };
	microPhysEnvDataResult.reset();
}

void dataClass::setMicroPhysicsData(const microPhysicsParams* params)
{
	microPhysicsParams paramsCPU;
	cudaMemcpy(&paramsCPU, params, sizeof(microPhysicsParams), cudaMemcpyDeviceToHost);

	microPhysEnvDataResult.PVCON[microPhysEnvDataResult.atIndex] = paramsCPU.PVCON;
	microPhysEnvDataResult.PVDEP[microPhysEnvDataResult.atIndex] = paramsCPU.PVDEP;
	microPhysEnvDataResult.PIMLT[microPhysEnvDataResult.atIndex] = paramsCPU.PIMLT;
	microPhysEnvDataResult.PIDW[microPhysEnvDataResult.atIndex] = paramsCPU.PIDW;
	microPhysEnvDataResult.PIHOM[microPhysEnvDataResult.atIndex] = paramsCPU.PIHOM;
	microPhysEnvDataResult.PIACR[microPhysEnvDataResult.atIndex] = paramsCPU.PIACR;
	microPhysEnvDataResult.PRACI[microPhysEnvDataResult.atIndex] = paramsCPU.PRACI;
	microPhysEnvDataResult.PRAUT[microPhysEnvDataResult.atIndex] = paramsCPU.PRAUT;
	microPhysEnvDataResult.PRACW[microPhysEnvDataResult.atIndex] = paramsCPU.PRACW;
	microPhysEnvDataResult.PREVP[microPhysEnvDataResult.atIndex] = paramsCPU.PREVP;
	microPhysEnvDataResult.PRACS[microPhysEnvDataResult.atIndex] = paramsCPU.PRACS;
	microPhysEnvDataResult.PSACW[microPhysEnvDataResult.atIndex] = paramsCPU.PSACW;
	microPhysEnvDataResult.PSACR[microPhysEnvDataResult.atIndex] = paramsCPU.PSACR;
	microPhysEnvDataResult.PSACI[microPhysEnvDataResult.atIndex] = paramsCPU.PSACI;
	microPhysEnvDataResult.PSAUT[microPhysEnvDataResult.atIndex] = paramsCPU.PSAUT;
	microPhysEnvDataResult.PSFW[microPhysEnvDataResult.atIndex] = paramsCPU.PSFW;
	microPhysEnvDataResult.PSFI[microPhysEnvDataResult.atIndex] = paramsCPU.PSFI;
	microPhysEnvDataResult.PSDEP[microPhysEnvDataResult.atIndex] = paramsCPU.PSDEP;
	microPhysEnvDataResult.PSSUB[microPhysEnvDataResult.atIndex] = paramsCPU.PSSUB;
	microPhysEnvDataResult.PSMLT[microPhysEnvDataResult.atIndex] = paramsCPU.PSMLT;
	microPhysEnvDataResult.PGAUT[microPhysEnvDataResult.atIndex] = paramsCPU.PGAUT;
	microPhysEnvDataResult.PGFR[microPhysEnvDataResult.atIndex] = paramsCPU.PGFR;
	microPhysEnvDataResult.PGACW[microPhysEnvDataResult.atIndex] = paramsCPU.PGACW;
	microPhysEnvDataResult.PGACI[microPhysEnvDataResult.atIndex] = paramsCPU.PGACI;
	microPhysEnvDataResult.PGACR[microPhysEnvDataResult.atIndex] = paramsCPU.PGACR;
	microPhysEnvDataResult.PGDRY[microPhysEnvDataResult.atIndex] = paramsCPU.PGDRY;
	microPhysEnvDataResult.PGACS[microPhysEnvDataResult.atIndex] = paramsCPU.PGACS;
	microPhysEnvDataResult.PGSUB[microPhysEnvDataResult.atIndex] = paramsCPU.PGSUB;
	microPhysEnvDataResult.PGMLT[microPhysEnvDataResult.atIndex] = paramsCPU.PGMLT;
	microPhysEnvDataResult.PGWET[microPhysEnvDataResult.atIndex] = paramsCPU.PGWET;
	microPhysEnvDataResult.PGACR1[microPhysEnvDataResult.atIndex] = paramsCPU.PGACR1;

	microPhysEnvDataResult.PVVAP[microPhysEnvDataResult.atIndex] = paramsCPU.PVVAP;
	microPhysEnvDataResult.PVSUB[microPhysEnvDataResult.atIndex] = paramsCPU.PVSUB;
}



void dataClass::drawMicroPhysGraph()
{
	microPhysEnvDataResult.atIndex++;
	if (microPhysEnvDataResult.atIndex >= MAXGRAPHLENGTH)
	{
		microPhysEnvDataResult.atIndex = 0;
	}

	//const float yMin = microPhysEnvDataResult.smallest;
	//const float yMax = microPhysEnvDataResult.biggest;

	if (ImPlot::BeginPlot("MicroPhysicsGraph"))
	{
		ImPlot::SetupLegend(ImPlotLocation_East, ImPlotLegendFlags_Outside);
		ImPlot::SetupAxes("Frame", "MixingRatio");

		//Adding all values to the graph
		addValueToGraph("PVCON", microPhysEnvDataResult.PVCON);
		addValueToGraph("PVDEP", microPhysEnvDataResult.PVDEP);
		addValueToGraph("PVVAP", microPhysEnvDataResult.PVVAP);
		addValueToGraph("PVSUB", microPhysEnvDataResult.PVSUB);
		addValueToGraph("PIMLT", microPhysEnvDataResult.PIMLT);
		addValueToGraph("PIDW", microPhysEnvDataResult.PIDW);
		addValueToGraph("PIHOM", microPhysEnvDataResult.PIHOM);
		addValueToGraph("PIACR", microPhysEnvDataResult.PIACR);
		addValueToGraph("PRACI", microPhysEnvDataResult.PRACI);
		addValueToGraph("PRAUT", microPhysEnvDataResult.PRAUT);
		addValueToGraph("PRACW", microPhysEnvDataResult.PRACW);
		addValueToGraph("PREVP", microPhysEnvDataResult.PREVP);
		addValueToGraph("PRACS", microPhysEnvDataResult.PRACS);
		addValueToGraph("PSACW", microPhysEnvDataResult.PSACW);
		addValueToGraph("PSACR", microPhysEnvDataResult.PSACR);
		addValueToGraph("PSACI", microPhysEnvDataResult.PSACI);
		addValueToGraph("PSAUT", microPhysEnvDataResult.PSAUT);
		addValueToGraph("PSFW", microPhysEnvDataResult.PSFW);
		addValueToGraph("PSFI", microPhysEnvDataResult.PSFI);
		addValueToGraph("PSDEP", microPhysEnvDataResult.PSDEP);
		addValueToGraph("PSSUB", microPhysEnvDataResult.PSSUB);
		addValueToGraph("PSMLT", microPhysEnvDataResult.PSMLT);
		addValueToGraph("PGAUT", microPhysEnvDataResult.PGAUT);
		addValueToGraph("PGFR", microPhysEnvDataResult.PGFR);
		addValueToGraph("PGACW", microPhysEnvDataResult.PGACW);
		addValueToGraph("PGACI", microPhysEnvDataResult.PGACI);
		addValueToGraph("PGACR", microPhysEnvDataResult.PGACR);
		addValueToGraph("PGDRY", microPhysEnvDataResult.PGDRY);
		addValueToGraph("PGACS", microPhysEnvDataResult.PGACS);
		addValueToGraph("PGSUB", microPhysEnvDataResult.PGSUB);
		addValueToGraph("PGMLT", microPhysEnvDataResult.PGMLT);
		addValueToGraph("PGWET", microPhysEnvDataResult.PGWET);
		addValueToGraph("PGACR1", microPhysEnvDataResult.PGACR1);

		//Add hovered detailed
		detailedLegendEntry("PVCON", "Condensation of vapor to cloud water", "Qw", "Qv");
		detailedLegendEntry("PVVAP", "Vaporization of cloud water to vapor", "Qv", "Qw");
		detailedLegendEntry("PVDEP", "Deposition of vapor to cloud ice", "Qc", "Qv");
		detailedLegendEntry("PVSUB", "Sublimation of cloud ice to vapor", "Qv", "Qc");
		detailedLegendEntry("PIMLT", "Melting of cloud ice to form cloud water", "Qw", "Qc", "If °C >= 0");
		detailedLegendEntry("PIDW", "Depositional growth of cloud ice at expense of cloud water", "Qc", "Qw", "If °C < 0");
		detailedLegendEntry("PIHOM", "Homogeneous freezing of cloud water to form cloud ice", "Qc", "Qw", "If °C < -40");
		detailedLegendEntry("PIACR", "Freezing of rain by cloud ice", "Qs, Qi", "Qr", "produces snow or graupel depending on the amount of rain");
		detailedLegendEntry("PRACI", "Removal of cloud ice by rain", "Qs, Qi", "Qc", "produces snow or graupel depending on the amount of rain");
		detailedLegendEntry("PRAUT", "Autoconversion of cloud water to form rain", "Qr", "Qw", "Point of autoconversion is 0.001 kg/kg");
		detailedLegendEntry("PRACW", "Accretion (growth) of rain by cloud water", "Qr", "Qw");
		detailedLegendEntry("PREVP", "Evaporation of rain", "Qv", "Qr");
		detailedLegendEntry("PRACS", "Accretion (growth) of hail by snow", "Qi", "Qs", "If °C >= 0 and Qr and Qs are smaller than 1e-4");
		detailedLegendEntry("PSACW", "Accretion (growth) snow or rain from cloud water", "Qs, Qr", "Qr, Qw", "If °C < 0, forms snow, If °C >= 0 forms rain and enhances smelting (PSMLT)");
		detailedLegendEntry("PSACR", "Freezing of rain to form hail/snow", "Qs, Qi", "Qr", "Produces graupel if rain exceeds 1e-4, If °C >= 0 enhances smelting (PSMLT)");
		detailedLegendEntry("PSACI", "Accetion (growth) of snow by cloud ice", "Qs", "Qc");
		detailedLegendEntry("PSAUT", "Autoconversion (aggregation) of cloud ice to form snow", "Qs", "Qc", "Happens when cloud ice exceeds 0.001 kg/kg");
		detailedLegendEntry("PSFW", "Bergeron process (deposition and riming) transfer of cloud water to form snow", "Qs", "Qw");
		detailedLegendEntry("PSFI", "Transfer rate of cloud ice to snow through growth of Bergeron process embryos", "Qs", "Qc");
		detailedLegendEntry("PSDEP", "Depositional growth of snow", "Qs", "Qv");
		detailedLegendEntry("PSSUB", "Sublimation of snow", "Qv", "Qs");
		detailedLegendEntry("PSMLT", "Melting of snow to form rain", "Qr", "Qs", "If °C >= 0");
		detailedLegendEntry("PGAUT", "Autoconversion (aggregation) of snow to form graupel", "Qi", "Qs", "Only when Qs exceeds 0.0006 kg/kg");
		detailedLegendEntry("PGFR", "Probalistic freezing of rain to form graupel", "Qi", "Qr");
		detailedLegendEntry("PGACW", "Accretion (growth) of graupel by cloud water", "Qi", "Qw", "Also used in PGWET and PGDRY");
		detailedLegendEntry("PGACI", "Accretion (growth) of graupel by cloud ice", "Qi", "Qc", "Also used in PGWET and PGDRY");
		detailedLegendEntry("PGACR", "Accretion (growth) of graupel by rain", "Qi", "Qr", "Also used in PGWET and PGDRY");
		detailedLegendEntry("PGACS", "Accretion (growth) of graupel by snow", "Qi", "Qs", "Also used in PGWET and PGDRY");
		detailedLegendEntry("PGDRY", "Dry growth of graupel", "Qi", "Qw, Qc, Qr, Qs", "Used if value is smaller than PGWET");
		detailedLegendEntry("PGSUB", "Sublimation of graupel", "Qv", "Qi");
		detailedLegendEntry("PGMLT", "Melting of graupel to form rain", "Qr", "Qi", "If °C >= 0");
		detailedLegendEntry("PGWET", "Wet growth of graupel", "Qi", "Qw, Qc, Qr, Qs", "Used if smaller than PGDRY, Is included in PGACR1");
		detailedLegendEntry("PGACR1", "Fallout or growth of hail by wetness", "Qi, Qr", "Qi, Qr", "If < 0, we add to rain since water will be sheded off, if >= 0, we remove from rain since some will be frozen");

		ImPlot::EndPlot();
	}
}

void dataClass::addValueToGraph(const char* name, const float* value)
{
	if (ImPlot::IsLegendEntryHovered(name))
	{
		ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 3.0f);
		ImPlot::PushStyleVar(ImPlotStyleVar_FillAlpha, 0.35f);
	}
	else
	{
		ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 3.0f);
		ImPlot::PushStyleVar(ImPlotStyleVar_FillAlpha, 0.15f);
	}
	ImPlot::PlotShaded(name, value, MAXGRAPHLENGTH);
	ImPlot::PopStyleVar();
	ImPlot::PlotLine(name, value, MAXGRAPHLENGTH);
	ImPlot::PopStyleVar(); //Pop line thickness
}

void dataClass::detailedLegendEntry(const char* label, const char* details, const char* addsTo, const char* removesFrom, const char* extra)
{
	if (ImPlot::IsLegendEntryHovered(label))
	{
		ImGui::BeginTooltip();
		ImGui::Text(label);
		ImGui::Separator();
		ImGui::Text(details);
		if (ImGui::BeginTable("table1", 2))
		{
			ImGui::TableSetupColumn("add (+)");
			ImGui::TableSetupColumn("remove (-)");
			ImGui::TableHeadersRow();
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text(addsTo);
			ImGui::TableSetColumnIndex(1);
			ImGui::Text(removesFrom);
			ImGui::EndTable();
		}
		ImGui::Text(extra);

		ImGui::EndTooltip();
	}

}
