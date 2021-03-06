// Copyright (c) 2020 Can Boluk and contributors of the VTIL Project   
// All rights reserved.   
//    
// Redistribution and use in source and binary forms, with or without   
// modification, are permitted provided that the following conditions are met: 
//    
// 1. Redistributions of source code must retain the above copyright notice,   
//    this list of conditions and the following disclaimer.   
// 2. Redistributions in binary form must reproduce the above copyright   
//    notice, this list of conditions and the following disclaimer in the   
//    documentation and/or other materials provided with the distribution.   
// 3. Neither the name of mosquitto nor the names of its   
//    contributors may be used to endorse or promote products derived from   
//    this software without specific prior written permission.   
//    
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE   
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE  
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE   
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR   
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF   
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS   
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN   
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)   
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE  
// POSSIBILITY OF SUCH DAMAGE.        
//
#pragma once
#include <algorithm>
#include <array>
#include "concept.hpp"
#include "..\io\formatting.hpp"

namespace vtil
{
	// Defines a 128-bit hash type.
	//
	class hash_t
	{
		// Magic constants for 128-bit FNV-1 .
		//
		using value_t = std::array<size_t, 2>;
		static constexpr value_t default_seed = { 0x6C62272E07BB0142, 0x62B821756295C58D };
		static constexpr value_t prime =        { 0x0000000001000000, 0x000000000000013B };

		// Current value of the hash.
		//
		value_t value;

	public:
		// Construct a new hash from an optional seed of either 64-bit or 128-bit value.
		//
		hash_t( size_t seed64 ) : value( { ~0ull, seed64 } ) {}
		hash_t( value_t seed128 = default_seed ) : value( seed128 ) {}

		// Append the given item into hash.
		//
		template<typename T>
		hash_t& operator<<( const T& item )
		{
			// Parse the item as an array of bytes.
			//
			auto& bytes = ( const uint8_t(&)[ sizeof(T) ] ) item;

			// Apply the FNV-1 algorithm and return self-reference.
			//
			for ( uint8_t byte : bytes )
			{
				// XOR over the low bytes.
				//
				value[ 1 ] ^= byte;

				// Calculate [value * prime].
				//
				// A: 0x???????????????? 0x????????????????
				//                    HA                 LA
				uint64_t ha = value[ 0 ], la = value[ 1 ];
				// B: 0x0000000001000000 0x000000000000013B
				//                    HB                 LB
				uint64_t hb = prime[ 0 ], lb = prime[ 1 ];
				//                                        x
				// ----------------------------------------
				// = (HA<<64 + LA) * (HB<<64 + LB)
				//
				// = LA     * LB       (Has both low and high parts)
				//
				value[ 1 ] = _umul128( la, lb, &value[ 0 ] );
				//
				//   HA<<64 * HB<<64 + (Discarded)
				//   HA<<64 * LB     + (Will have no low part)
				//
				value[ 0 ] += ha * lb;
				//
				//   LA     * HB<<64 + (Will have no low part)
				//
				value[ 0 ] += la * hb;
			}

			// Return a self-reference.
			//
			return *this;
		}

		// Implicit conversion to 64-bit and 128-bit values.
		//
		size_t as64() const { return value[ 0 ] + value[ 1 ]; }
		value_t as128() const { return value; }
		operator size_t() const { return as64(); }
		operator value_t() const { return as128(); }
		
		// Conversion to human-readable format.
		//
		std::string to_string() const
		{
			return format::str( "0x%p%p", value[ 1 ], value[ 0 ] );
		}

		// Basic comparison operators.
		//
		bool operator<( const hash_t& o ) const { return value < o.value; }
		bool operator==( const hash_t& o ) const { return value == o.value; }
		bool operator!=( const hash_t& o ) const { return value != o.value; }
	};

	// Default hasher of VTIL objects, the type should export a public 
	// function with the signature [hash_t hash() const].
	//
	template<typename T>
	struct hash
	{
		hash_t operator()( const T& value ) const { return value.hash(); }
	};

	// Check if type is hashable using std::hash.
	//
	template<typename... D>
	struct is_std_hashable : concept_base<is_std_hashable, D...>
	{
		template<typename T>
		static auto f( T v ) -> decltype( std::hash<std::remove_cvref_t<T>>{}( v ) );
	};

	// Check if type is hashable using vtil::hash.
	//
	template<typename... D>
	struct is_vtil_hashable : concept_base<is_vtil_hashable, D...>
	{
		template<typename T>
		static auto f( const T v ) -> std::enable_if_t<std::is_same_v<decltype( v.hash() ), hash_t>>;
	};

	// Checks if the type is hashable using any hasher.
	//
	template<typename T>
	constexpr bool is_hashable_v = is_vtil_hashable<T>::apply() || is_std_hashable<T>::apply();

	// Resolves the default hasher of the type, void if none.
	//
	template<typename T>
	using default_hasher_t =
		std::conditional_t<is_vtil_hashable<T>::apply(), vtil::hash<T>,
		std::conditional_t<is_std_hashable<T>::apply(), std::hash<T>, void>>;
};

// Make vtil::hash_t std::hashable.
//
namespace std
{
	template<>
	struct hash<vtil::hash_t>
	{
		size_t operator()( const vtil::hash_t& value ) const { return value; }
	};
};