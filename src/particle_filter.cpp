/*
 * particle_filter.cpp
 *
 *  Created on: Dec 12, 2016
 *  Framework Author: Tiffany Huang
 *  Algorithm Implementation: Hesen Zhang
 */

#include <random>
#include <algorithm>
#include <iostream>
#include <numeric>
#include <math.h> 
#include <iostream>
#include <sstream>
#include <string>
#include <iterator>
#include <random>
#include "particle_filter.h"

#define EPS 0.00001

using namespace std;

std::default_random_engine gen;


void ParticleFilter::init(double x, double y, double theta, double std[]) {
	// TODO: Set the number of particles. Initialize all particles to first position (based on estimates of 
	//   x, y, theta and their uncertainties from GPS) and all weights to 1. 
	// Add random Gaussian noise to each particle.
	// NOTE: Consult particle_filter.h for more information about this method (and others in this file).
  if (is_initialized) {
    return;
  }

  // Initializing the number of particles
  num_particles = 100;

  // Extracting standard deviations
  double std_x = std[0];
  double std_y = std[1];
  double std_theta = std[2];

  // Creating normal distributions
  normal_distribution<double> dist_x(x, std_x);
  normal_distribution<double> dist_y(y, std_y);
  normal_distribution<double> dist_theta(theta, std_theta);

  // Generate particles with normal distribution with mean on GPS values.
  for (int i = 0; i < num_particles; i++) {

    Particle particle;
    particle.id = i;
    particle.x = dist_x(gen);
    particle.y = dist_y(gen);
    particle.theta = dist_theta(gen);
    particle.weight = 1.0;

    particles.push_back(particle);
	}

  // The filter is now initialized.
  is_initialized = true;
}

void ParticleFilter::prediction(double delta_t, double std_pos[], double velocity, double yaw_rate) {
	// TODO: Add measurements to each particle and add random Gaussian noise.
	// NOTE: When adding noise you may find std::normal_distribution and std::default_random_engine useful.
	//  http://en.cppreference.com/w/cpp/numeric/random/normal_distribution
	//  http://www.cplusplus.com/reference/random/default_random_engine/

	double std_x = std_pos[0];
	double std_y = std_pos[1];
	double std_theta = std_pos[2];

	normal_distribution<double> dist_x(0, std_x);
	normal_distribution<double> dist_y(0, std_y);
	normal_distribution<double> dist_theta(0, std_theta);

	  // Calculate new state.
	  for (int i = 0; i < num_particles; i++) {

	  	double theta = particles[i].theta;

	    if ( fabs(yaw_rate) < EPS ) { // When yaw is not changing.
	      particles[i].x += velocity * delta_t * cos( theta );
	      particles[i].y += velocity * delta_t * sin( theta );
	      // yaw continue to be the same.
	    } else {
	      particles[i].x += velocity / yaw_rate * ( sin( theta + yaw_rate * delta_t ) - sin( theta ) );
	      particles[i].y += velocity / yaw_rate * ( cos( theta ) - cos( theta + yaw_rate * delta_t ) );
	      particles[i].theta += yaw_rate * delta_t;
	    }

	    // Adding noise.
	    particles[i].x += dist_x(gen);
	    particles[i].y += dist_y(gen);
	    particles[i].theta += dist_theta(gen);
	  }
 
}

void ParticleFilter::dataAssociation(std::vector<LandmarkObs> predicted, std::vector<LandmarkObs>& observations) {
	// TODO: Find the predicted measurement that is closest to each observed measurement and assign the 
	//   observed measurement to this particular landmark.
	// NOTE: this method will NOT be called by the grading code. But you will probably find it useful to 
	//   implement this method and use it as a helper during the updateWeights phase.


	for (auto & o : observations) { // For each observation
		double min_distance = numeric_limits<double>::max();
		int map_id = -1;
		for (auto & p : predicted) { // For each predition.
			double dx = o.x - p.x;
			double dy = o.y - p.y;
			double distance = dy * dy + dx * dx;
			// If the "distance" is less than min, stored the id and update min.
      		if ( distance < min_distance ) {
        		min_distance = distance;
        		map_id = p.id;
      		}

			min_distance = min(min_distance, distance);
		}
		o.id = map_id;
	}
}

void ParticleFilter::updateWeights(double sensor_range, double std_landmark[], 
		const std::vector<LandmarkObs> &observations, const Map & map_landmarks) {
	// TODO: Update the weights of each particle using a mult-variate Gaussian distribution. You can read
	//   more about this distribution here: https://en.wikipedia.org/wiki/Multivariate_normal_distribution
	// NOTE: The observations are given in the VEHICLE'S coordinate system. Your particles are located
	//   according to the MAP'S coordinate system. You will need to transform between the two systems.
	//   Keep in mind that this transformation requires both rotation AND translation (but no scaling).
	//   The following is a good resource for the theory:
	//   https://www.willamette.edu/~gorr/classes/GeneralGraphics/Transforms/transforms2d.htm
	//   and the following is a good resource for the actual equation to implement (look at equation 
	//   3.33
	//   http://planning.cs.uiuc.edu/node99.html


	for (auto & p : particles) {

		vector<LandmarkObs> inRangeLandmarks;

		for(const auto & landmark : map_landmarks.landmark_list) {
			if ( fabs(p.x - landmark.x_f) <= sensor_range &&  fabs(p.y - landmark.y_f) <= sensor_range) {
				inRangeLandmarks.emplace_back( LandmarkObs{landmark.id_i, landmark.x_f, landmark.y_f} );
			}
		}

		vector<LandmarkObs> transformed_observations;
		for(auto & o : observations) {
			double x = cos(p.theta)*o.x - sin(p.theta)*o.y + p.x;
			double y = sin(p.theta)*o.x + cos(p.theta)*o.y + p.y;
			transformed_observations.emplace_back(LandmarkObs{o.id, x, y});
		}

		dataAssociation(inRangeLandmarks, transformed_observations);

		p.weight = 1.0;
		for(auto & o : transformed_observations) {

			int landmarkId = o.id;
      		double landmarkX, landmarkY;
      		unsigned int k = 0;
      		unsigned int nLandmarks = inRangeLandmarks.size();
      		bool found = false;
      		while( !found && k < nLandmarks ) {
        		if ( inRangeLandmarks[k].id == landmarkId) {
          			found = true;
          			landmarkX = inRangeLandmarks[k].x;
          			landmarkY = inRangeLandmarks[k].y;
        		}
        		k++;
      		}

			double dx = o.x - landmarkX;
			double dy = o.y - landmarkY;
			// Calculating weight.
			double weight = (1 / (2 * M_PI * std_landmark[0] * std_landmark[1])) * 
				exp( -( dx * dx / (2 * std_landmark[0] * std_landmark[0] ) + 
							( dy * dy / (2 * std_landmark[1] * std_landmark[1])) ) 
					 );
			p.weight *= weight;

		}
	}
}

void ParticleFilter::resample() {
	// TODO: Resample particles with replacement with probability proportional to their weight. 
	// NOTE: You may find std::discrete_distribution helpful here.
	// http://en.cppreference.com/w/cpp/numeric/random/discrete_distribution
	vector<Particle> resampled_particles;

	uniform_int_distribution<int> index_dist(0, num_particles - 1);
	auto cur_index = index_dist(gen);

	double beta = 0.0;

  	double max_weight = numeric_limits<double>::min();
  	for(int i = 0; i < num_particles; i++) {
    	if ( particles[i].weight > max_weight ) {
      	max_weight = particles[i].weight;
    	}
  	}

	uniform_real_distribution<double> beta_dist(0.0, max_weight);

	for (int i = 0; i < num_particles; i++) {
		beta += beta_dist(gen) * 2.0;
		while (beta > particles[cur_index].weight) {
			beta -= particles[cur_index].weight;
			cur_index = (cur_index + 1) % num_particles;
		}
		resampled_particles.emplace_back(particles[cur_index]);
	}
	particles = resampled_particles;
}

Particle ParticleFilter::SetAssociations(Particle& particle, const std::vector<int>& associations, 
		const std::vector<double>& sense_x, const std::vector<double>& sense_y)
{
	//particle: the particle to assign each listed association, and association's (x,y) world coordinates mapping to
	// associations: The landmark id that goes along with each listed association
	// sense_x: the associations x mapping already converted to world coordinates
	// sense_y: the associations y mapping already converted to world coordinates

	particle.associations = associations;
	particle.sense_x = sense_x;
	particle.sense_y = sense_y;

	return particle;
}

string ParticleFilter::getAssociations(Particle best)
{
	vector<int> v = best.associations;
	stringstream ss;
	copy( v.begin(), v.end(), ostream_iterator<int>(ss, " "));
	string s = ss.str();
	s = s.substr(0, s.length()-1);  // get rid of the trailing space
	return s;
}
string ParticleFilter::getSenseX(Particle best)
{
	vector<double> v = best.sense_x;
	stringstream ss;
	copy( v.begin(), v.end(), ostream_iterator<float>(ss, " "));
	string s = ss.str();
	s = s.substr(0, s.length()-1);  // get rid of the trailing space
	return s;
}
string ParticleFilter::getSenseY(Particle best)
{
	vector<double> v = best.sense_y;
	stringstream ss;
	copy( v.begin(), v.end(), ostream_iterator<float>(ss, " "));
	string s = ss.str();
	s = s.substr(0, s.length()-1);  // get rid of the trailing space
	return s;
}
